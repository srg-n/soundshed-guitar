#include "NAMGuitarPlugin.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <optional>
#include <vector>
#include <exception>
#include <string_view>

#include <nlohmann/json.hpp>

#include "config.h"
#include "IControls.h"
#include "IPlug_include_in_plug_src.h"
#include "IPlugPaths.h"
#include "wdlstring.h"

#include "dsp/NAMDSPManager.h"
#include "ui/WebUIBridge.h"

namespace namguitar
{
namespace
{
constexpr int kNumPrograms = 0;

std::string ParamKey(NAMGuitarPlugin::ParameterId paramId);

nlohmann::json SerializeParametersToJson(const NAMGuitarPlugin& plugin)
{
  nlohmann::json parameters = nlohmann::json::array();
  for (int paramIdx = 0; paramIdx < NAMGuitarPlugin::kParamCount; ++paramIdx)
  {
    const auto* param = plugin.GetParam(paramIdx);
    nlohmann::json paramJson;
    std::string key = ParamKey(static_cast<NAMGuitarPlugin::ParameterId>(paramIdx));
    if (key.empty())
    {
      key = param->GetName();
    }
    paramJson["id"] = std::move(key);
    paramJson["value"] = param->Value();
    paramJson["label"] = param->GetName();
    parameters.push_back(std::move(paramJson));
  }

  nlohmann::json payload;
  payload["parameters"] = std::move(parameters);
  payload["gateEnabled"] = plugin.GetParam(NAMGuitarPlugin::kParamGateEnabled)->Bool();
  payload["gateThreshold"] = plugin.GetParam(NAMGuitarPlugin::kParamGateThreshold)->Value();
  return payload;
}

std::string ParamKey(NAMGuitarPlugin::ParameterId paramId)
{
  switch (paramId)
  {
    case NAMGuitarPlugin::kParamInputTrim:
      return "input_trim";
    case NAMGuitarPlugin::kParamOutputTrim:
      return "output_trim";
    case NAMGuitarPlugin::kParamDrive:
      return "drive";
    case NAMGuitarPlugin::kParamTone:
      return "tone";
    case NAMGuitarPlugin::kParamGateEnabled:
      return "gate_enabled";
    case NAMGuitarPlugin::kParamGateThreshold:
      return "gate_threshold";
    default:
      return "";
  }
}

std::optional<NAMGuitarPlugin::ParameterId> ParamIdFromKey(const std::string& key)
{
  if (key == "input_trim")
  {
    return NAMGuitarPlugin::kParamInputTrim;
  }
  if (key == "output_trim")
  {
    return NAMGuitarPlugin::kParamOutputTrim;
  }
  if (key == "drive")
  {
    return NAMGuitarPlugin::kParamDrive;
  }
  if (key == "tone")
  {
    return NAMGuitarPlugin::kParamTone;
  }
  if (key == "gate_enabled")
  {
    return NAMGuitarPlugin::kParamGateEnabled;
  }
  if (key == "gate_threshold")
  {
    return NAMGuitarPlugin::kParamGateThreshold;
  }
  return std::nullopt;
}

nlohmann::json SerializePresetToJson(const Preset& preset)
{
  nlohmann::json jsonPreset;
  jsonPreset["id"] = preset.id;
  jsonPreset["name"] = preset.name;
  jsonPreset["category"] = preset.category;
  jsonPreset["description"] = preset.description;
  jsonPreset["namModelId"] = preset.namModelId;
  jsonPreset["irId"] = preset.irId;
  jsonPreset["fxChain"] = preset.fxChain;

  nlohmann::json attachments = nlohmann::json::array();
  for (const auto& attachment : preset.attachments)
  {
    nlohmann::json attachmentJson;
    attachmentJson["type"] = attachment.type;
    attachmentJson["filePath"] = attachment.filePath.generic_string();
    attachmentJson["hash"] = attachment.hash;
    attachments.push_back(std::move(attachmentJson));
  }
  jsonPreset["attachments"] = std::move(attachments);

  nlohmann::json parameters = nlohmann::json::array();
  for (const auto& parameter : preset.parameters)
  {
    parameters.push_back({
      {"id", parameter.id},
      {"value", parameter.value},
    });
  }
  jsonPreset["parameters"] = std::move(parameters);

  return jsonPreset;
}

} // namespace

NAMGuitarPlugin::NAMGuitarPlugin(const iplug::InstanceInfo& info)
  : iplug::Plugin(info, iplug::MakeConfig(kParamCount, kNumPrograms))
  , mDSP(std::make_unique<NAMDSPManager>())
  , mWebUI(std::make_unique<WebUIBridge>())
{
  InitializeParameters();
  HandleWebViewMessages();

  for (int paramIdx = 0; paramIdx < kParamCount; ++paramIdx)
  {
    OnParamChange(paramIdx);
  }

#if PLUG_HAS_UI
  mMakeGraphicsFunc = [this]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [this](iplug::igraphics::IGraphics* graphics) {
    if (!graphics)
    {
      return;
    }
    graphics->AttachPanelBackground(iplug::igraphics::COLOR_BLACK);
    InitializeGraphics(*graphics);
  };
#endif
}

void NAMGuitarPlugin::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  if (!mDSP)
  {
    return;
  }

  mDSP->Process(inputs, outputs, nFrames);
}

void NAMGuitarPlugin::OnReset()
{
  if (!mDSP)
  {
    return;
  }

  mDSP->Prepare(GetSampleRate(), GetBlockSize());
}

void NAMGuitarPlugin::OnIdle()
{
  if (mWebUI)
  {
    mWebUI->PumpMessages();
  }

  if (mPendingStateBroadcast)
  {
    BroadcastState();
  }
}

bool NAMGuitarPlugin::SerializeState(iplug::IByteChunk& chunk) const
{
  bool success = chunk.PutStr(mActivePresetJson.c_str());
  success &= chunk.PutStr(mActivePresetId.c_str());
  success &= chunk.PutStr(mActiveModelPath.c_str());
  success &= chunk.PutStr(mActiveIRPath.c_str());
  return success;
}

int NAMGuitarPlugin::UnserializeState(const iplug::IByteChunk& chunk, int startPos)
{
  int position = startPos;

  WDL_String presetJson;
  position = chunk.GetStr(presetJson, position);
  if (position < 0)
  {
    return startPos;
  }
  mActivePresetJson = presetJson.Get();

  WDL_String activePresetId;
  position = chunk.GetStr(activePresetId, position);
  if (position < 0)
  {
    return startPos;
  }
  mActivePresetId = activePresetId.Get();

  WDL_String modelPath;
  position = chunk.GetStr(modelPath, position);
  if (position < 0)
  {
    return startPos;
  }
  mActiveModelPath = modelPath.Get();

  WDL_String irPath;
  position = chunk.GetStr(irPath, position);
  if (position < 0)
  {
    return startPos;
  }
  mActiveIRPath = irPath.Get();

  if (!mActivePresetJson.empty())
  {
    if (!nlohmann::json::accept(mActivePresetJson))
    {
      ReportErrorToUI("Failed to restore preset state", "Saved preset JSON is invalid");
    }
    else
    {
      const auto jsonPreset = nlohmann::json::parse(mActivePresetJson);
      if (!jsonPreset.is_object())
      {
        ReportErrorToUI("Failed to restore preset state", "Preset JSON is not an object");
      }
      else
      {
        Preset preset = ParsePresetFromJson(jsonPreset);
        ApplyPreset(preset);
        mActivePreset = preset;
        if (mActivePresetId.empty())
        {
          mActivePresetId = preset.id;
        }
      }
    }
  }

  mPendingStateBroadcast = true;
  return position;
}

void NAMGuitarPlugin::OnParamChange(int paramIdx)
{
  if (!mDSP)
  {
    return;
  }

  const auto* param = GetParam(paramIdx);
  if (!param)
  {
    return;
  }

  switch (static_cast<ParameterId>(paramIdx))
  {
    case kParamInputTrim:
      mDSP->SetInputTrim(param->Value());
      break;
    case kParamOutputTrim:
      mDSP->SetOutputTrim(param->Value());
      break;
    case kParamDrive:
      mDSP->SetDrive(param->Value());
      break;
    case kParamTone:
      mDSP->SetTone(param->Value() * 2.0 - 1.0);
      break;
    case kParamGateEnabled:
      mDSP->SetGateEnabled(param->Bool());
      break;
    case kParamGateThreshold:
      mDSP->SetGateThreshold(param->Value());
      break;
    default:
      break;
  }

  mPendingStateBroadcast = true;
}

void NAMGuitarPlugin::InitializeParameters()
{
  GetParam(kParamInputTrim)->InitDouble("Input Trim", 0.0, -24.0, 12.0, 0.1, "dB");
  GetParam(kParamOutputTrim)->InitDouble("Output Trim", 0.0, -24.0, 12.0, 0.1, "dB");
  GetParam(kParamDrive)->InitDouble("Drive", 0.5, 0.0, 1.0, 0.01);
  GetParam(kParamTone)->InitDouble("Tone Tilt", 0.5, 0.0, 1.0, 0.01);
  GetParam(kParamGateEnabled)->InitBool("Noise Gate", false);
  GetParam(kParamGateThreshold)->InitDouble("Gate Threshold", -60.0, -80.0, -20.0, 0.1, "dB");
}

void NAMGuitarPlugin::HandleWebViewMessages()
{
  if (!mWebUI)
  {
    return;
  }

  mWebUI->RegisterMessageHandler([this](const std::string& message) {
    HandleUIMessage(message);
  });

  mWebUI->RegisterLogHandler([this](const std::string& logMessage) {
    (void) logMessage;
  });
}

void NAMGuitarPlugin::InitializeGraphics(iplug::igraphics::IGraphics& graphics)
{
  if (!mWebUI)
  {
    return;
  }

  WDL_String bundlePath;
  iplug::BundleResourcePath(bundlePath, ::gHINSTANCE);
  if (bundlePath.GetLength() == 0)
  {
    iplug::HostPath(bundlePath, nullptr);
    bundlePath.Append("resources\\");
  }
  const std::filesystem::path resourceRoot = std::filesystem::path{bundlePath.Get()};
  mWebUI->Initialize(graphics, resourceRoot);
  mPendingStateBroadcast = true;
}

void NAMGuitarPlugin::HandleUIMessage(const std::string& message)
{
  if (!nlohmann::json::accept(message))
  {
    ReportErrorToUI("Received invalid message", "UI sent malformed JSON payload");
    return;
  }

  const auto payload = nlohmann::json::parse(message);
  if (!payload.is_object())
  {
    ReportErrorToUI("Received invalid message", "UI sent malformed JSON payload");
    return;
  }

  const std::string type = payload.value("type", "");
  if (type == "loadPreset")
  {
    HandlePresetLoadRequest(payload);
  }
  else if (type == "requestState")
  {
    HandleStateRequest();
  }
  else if (type == "runSignalPathTest")
  {
    HandleSignalTestRequest(payload);
  }
}

void NAMGuitarPlugin::BroadcastState()
{
  if (!mWebUI)
  {
    return;
  }

  nlohmann::json message;
  message["type"] = "state";
  message["activePresetId"] = mActivePresetId;

  auto parameters = SerializeParametersToJson(*this);
  parameters["modelPath"] = mActiveModelPath;
  parameters["irPath"] = mActiveIRPath;
  message["parameters"] = std::move(parameters);

  if (mActivePreset)
  {
    message["preset"] = SerializePresetToJson(*mActivePreset);
  }

  mWebUI->EnqueueMessage(message.dump());
  mPendingStateBroadcast = false;
}

void NAMGuitarPlugin::ApplyPreset(Preset& preset)
{
  if (!mDSP)
  {
    return;
  }

  for (const auto& parameter : preset.parameters)
  {
    const auto paramId = ParamIdFromKey(parameter.id);
    if (paramId)
    {
      const auto index = static_cast<int>(*paramId);
      auto* param = GetParam(index);
      if (param)
      {
        param->Set(parameter.value);
        OnParamChange(index);
      }
    }
  }

  for (auto& attachment : preset.attachments)
  {
    const auto resolvedPath = MaterializeAttachment(attachment);
    if (!resolvedPath)
    {
      continue;
    }

    attachment.filePath = *resolvedPath;
    attachment.data.clear();

    if (attachment.type == "nam")
    {
      if (mDSP->LoadModel(*resolvedPath))
      {
        mActiveModelPath = resolvedPath->generic_string();
      }
    }
    else if (attachment.type == "ir")
    {
      if (mDSP->LoadImpulseResponse(*resolvedPath))
      {
        mActiveIRPath = resolvedPath->generic_string();
      }
    }
  }
}

void NAMGuitarPlugin::HandlePresetLoadRequest(const nlohmann::json& payload)
{
  const auto presetJsonIter = payload.find("preset");
  if (presetJsonIter == payload.end() || !presetJsonIter->is_object())
  {
    return;
  }

  try
  {
    Preset preset = ParsePresetFromJson(*presetJsonIter);
    ApplyPreset(preset);

    mActivePreset = preset;
    mActivePresetId = preset.id;
    mActivePresetJson = presetJsonIter->dump();
    mPendingStateBroadcast = true;

    if (mWebUI)
    {
      nlohmann::json message;
      message["type"] = "presetLoaded";
      message["preset"] = SerializePresetToJson(preset);
      mWebUI->EnqueueMessage(message.dump());
    }
  }
  catch (const std::exception& exception)
  {
    mPendingStateBroadcast = true;
    ReportErrorToUI("Failed to load preset", exception.what());
  }
  catch (...)
  {
    mPendingStateBroadcast = true;
    ReportErrorToUI("Failed to load preset", "An unknown error occurred");
  }
}

void NAMGuitarPlugin::HandleStateRequest()
{
  mPendingStateBroadcast = true;
}

void NAMGuitarPlugin::HandleSignalTestRequest(const nlohmann::json& payload)
{
  const double frequency = payload.value("frequency", 440.0);
  const double duration = payload.value("duration", 1.0);

  const auto result = RunSignalPathTest(frequency, duration);

  if (!mWebUI)
  {
    return;
  }

  nlohmann::json message;
  message["type"] = "signalPathTestResult";
  message["frequency"] = result.frequencyHz;
  message["duration"] = result.durationSeconds;
  message["sampleRate"] = result.sampleRate;
  message["inputRMS"] = result.inputRMS;
  message["outputRMS"] = {result.outputRMS[0], result.outputRMS[1]};
  message["passed"] = result.passed;

  if (!result.passed)
  {
    message["message"] = "Signal path test did not produce any output";
  }

  mWebUI->EnqueueMessage(message.dump());
}

void NAMGuitarPlugin::ReportErrorToUI(std::string_view message, std::string_view detail) const
{
  if (!mWebUI)
  {
    return;
  }

  nlohmann::json payload;
  payload["type"] = "error";
  payload["message"] = std::string{message};
  if (!detail.empty())
  {
    payload["detail"] = std::string{detail};
  }

  mWebUI->EnqueueMessage(payload.dump());
}

std::optional<std::filesystem::path> NAMGuitarPlugin::MaterializeAttachment(const PresetAttachment& attachment) const
{
  std::filesystem::path target = ResolveAttachmentTarget(attachment);
  if (target.empty())
  {
    return std::nullopt;
  }

  if (!attachment.data.empty())
  {
    const auto data = DecodeBase64(attachment.data);
    if (data.empty())
    {
      return std::nullopt;
    }

    if (!WriteFile(target, data))
    {
      return std::nullopt;
    }
  }

  if (!std::filesystem::exists(target))
  {
    return std::nullopt;
  }

  if (!attachment.hash.empty())
  {
    const auto hash = mHasher.HashFile(target);
    if (!hash.empty() && hash != attachment.hash)
    {
      return std::nullopt;
    }
  }

  return target;
}

std::filesystem::path NAMGuitarPlugin::ResolveAttachmentTarget(const PresetAttachment& attachment) const
{
  if (!attachment.filePath.empty())
  {
    if (attachment.filePath.is_absolute())
    {
      return attachment.filePath;
    }

    if (const auto presetDir = mFileSystem.EnsureDirectory(mFileSystem.ResolvePresetDirectory()))
    {
      return *presetDir / attachment.filePath;
    }

    return attachment.filePath;
  }

  if (!attachment.hash.empty())
  {
    if (const auto presetDir = mFileSystem.EnsureDirectory(mFileSystem.ResolvePresetDirectory()))
    {
      return *presetDir / (attachment.hash + (attachment.type.empty() ? std::string{} : std::string{"."} + attachment.type));
    }
  }

  return {};
}

std::vector<std::uint8_t> NAMGuitarPlugin::DecodeBase64(const std::string& encoded)
{
  static const std::array<int, 256> decodeTable = []() {
    std::array<int, 256> table{};
    table.fill(-1);
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (std::size_t idx = 0; idx < alphabet.size(); ++idx)
    {
      table[static_cast<unsigned char>(alphabet[idx])] = static_cast<int>(idx);
    }
    table[static_cast<unsigned char>('-')] = 62;
    table[static_cast<unsigned char>('_')] = 63;
    return table;
  }();

  std::vector<std::uint8_t> output;
  int accumulator = 0;
  int bits = -8;

  for (unsigned char c : encoded)
  {
    if (std::isspace(c))
    {
      continue;
    }

    if (c == '=')
    {
      break;
    }

    const int value = decodeTable[c];
    if (value < 0)
    {
      return {};
    }

    accumulator = (accumulator << 6) + value;
    bits += 6;
    if (bits >= 0)
    {
      output.push_back(static_cast<std::uint8_t>((accumulator >> bits) & 0xFF));
      bits -= 8;
    }
  }

  return output;
}

bool NAMGuitarPlugin::WriteFile(const std::filesystem::path& target, const std::vector<std::uint8_t>& data) const
{
  if (target.empty())
  {
    return false;
  }

  const auto parent = target.parent_path();
  if (!parent.empty())
  {
    if (!mFileSystem.EnsureDirectory(parent))
    {
      return false;
    }
  }

  std::ofstream output(target, std::ios::binary | std::ios::trunc);
  if (!output)
  {
    return false;
  }

  output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  return output.good();
}

Preset NAMGuitarPlugin::ParsePresetFromJson(const nlohmann::json& jsonPreset)
{
  Preset preset;
  preset.id = jsonPreset.value("id", "");
  preset.name = jsonPreset.value("name", "");
  preset.category = jsonPreset.value("category", "");
  preset.description = jsonPreset.value("description", "");
  preset.namModelId = jsonPreset.value("namModelId", "");
  preset.irId = jsonPreset.value("irId", "");

  if (jsonPreset.contains("fxChain") && jsonPreset["fxChain"].is_array())
  {
    for (const auto& fx : jsonPreset["fxChain"])
    {
      preset.fxChain.push_back(fx.get<std::string>());
    }
  }

  if (jsonPreset.contains("attachments") && jsonPreset["attachments"].is_array())
  {
    for (const auto& jsonAttachment : jsonPreset["attachments"])
    {
      PresetAttachment attachment;
      attachment.type = jsonAttachment.value("type", "");
      attachment.hash = jsonAttachment.value("hash", "");
      if (jsonAttachment.contains("filePath"))
      {
        attachment.filePath = jsonAttachment.value("filePath", "");
      }
      else if (jsonAttachment.contains("path"))
      {
        attachment.filePath = jsonAttachment.value("path", "");
      }
      attachment.data = jsonAttachment.value("data", "");
      preset.attachments.push_back(std::move(attachment));
    }
  }

  if (jsonPreset.contains("parameters") && jsonPreset["parameters"].is_array())
  {
    for (const auto& jsonParameter : jsonPreset["parameters"])
    {
      PresetParameter entry;
      entry.id = jsonParameter.value("id", "");
      entry.value = jsonParameter.value("value", 0.0);
      preset.parameters.push_back(entry);
    }
  }

  return preset;
}

NAMGuitarPlugin::SignalPathTestResult NAMGuitarPlugin::RunSignalPathTest(double frequencyHz, double durationSeconds)
{
  SignalPathTestResult result;
  result.frequencyHz = frequencyHz;
  result.durationSeconds = durationSeconds;

  if (!mDSP)
  {
    return result;
  }

  if (frequencyHz <= 0.0 || durationSeconds <= 0.0)
  {
    return result;
  }

  const double sampleRate = GetSampleRate() > 0.0 ? GetSampleRate() : 44100.0;
  result.sampleRate = sampleRate;

  const int hostBlockSize = GetBlockSize();
  constexpr int kTestBlockSize = 256;
  const int analysisBlockSize = std::max(hostBlockSize, kTestBlockSize);
  const int restoreBlockSize = hostBlockSize > 0 ? hostBlockSize : analysisBlockSize;

  mDSP->Prepare(sampleRate, analysisBlockSize);
  mDSP->Reset();

  const int totalFrames = static_cast<int>(std::ceil(durationSeconds * sampleRate));
  if (totalFrames <= 0)
  {
    mDSP->Reset();
    mDSP->Prepare(sampleRate, restoreBlockSize);
    return result;
  }

  std::vector<iplug::sample> inputLeft(static_cast<std::size_t>(analysisBlockSize));
  std::vector<iplug::sample> inputRight(static_cast<std::size_t>(analysisBlockSize));
  std::vector<iplug::sample> outputLeft(static_cast<std::size_t>(analysisBlockSize));
  std::vector<iplug::sample> outputRight(static_cast<std::size_t>(analysisBlockSize));

  const double twoPi = std::numbers::pi * 2.0;
  const double phaseIncrement = twoPi * frequencyHz / sampleRate;
  double phase = 0.0;

  double inputSumSquares = 0.0;
  double outputSumSquaresLeft = 0.0;
  double outputSumSquaresRight = 0.0;

  for (int processed = 0; processed < totalFrames; processed += analysisBlockSize)
  {
    const int framesThisPass = std::min(analysisBlockSize, totalFrames - processed);

    for (int frame = 0; frame < framesThisPass; ++frame)
    {
      // Feed a calibrated sine tone into both channels to probe the entire guitar chain.
      const double sample = std::sin(phase) * 0.5;
      phase += phaseIncrement;
      if (phase >= twoPi)
      {
        phase -= twoPi;
      }

      inputLeft[static_cast<std::size_t>(frame)] = static_cast<iplug::sample>(sample);
      inputRight[static_cast<std::size_t>(frame)] = static_cast<iplug::sample>(sample);
      outputLeft[static_cast<std::size_t>(frame)] = 0.0f;
      outputRight[static_cast<std::size_t>(frame)] = 0.0f;

      inputSumSquares += sample * sample;
    }

    iplug::sample* in[] = {inputLeft.data(), inputRight.data()};
    iplug::sample* out[] = {outputLeft.data(), outputRight.data()};
    mDSP->Process(in, out, framesThisPass);

    for (int frame = 0; frame < framesThisPass; ++frame)
    {
      const double left = static_cast<double>(outputLeft[static_cast<std::size_t>(frame)]);
      const double right = static_cast<double>(outputRight[static_cast<std::size_t>(frame)]);
      outputSumSquaresLeft += left * left;
      outputSumSquaresRight += right * right;
    }
  }

  const double divisor = std::max(1.0, static_cast<double>(totalFrames));
  result.inputRMS = std::sqrt(inputSumSquares / divisor);
  result.outputRMS[0] = std::sqrt(outputSumSquaresLeft / divisor);
  result.outputRMS[1] = std::sqrt(outputSumSquaresRight / divisor);
  result.passed = (result.outputRMS[0] > 1e-4) || (result.outputRMS[1] > 1e-4);

  mDSP->Reset();
  mDSP->Prepare(sampleRate, restoreBlockSize);

  return result;
}

} // namespace namguitar
