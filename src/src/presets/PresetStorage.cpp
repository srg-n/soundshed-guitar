#include "PresetStorage.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <utility>

#include "IPlugStructs.h"

namespace namguitar
{
namespace
{
std::vector<Preset> BuildDefaultPresets()
{
  auto presetParameters = [](double inputTrim, double outputTrim, double drive, double tone, double gateEnabled,
                              double gateThreshold) {
    std::vector<PresetParameter> parameters;
    parameters.push_back({"input_trim", inputTrim});
    parameters.push_back({"output_trim", outputTrim});
    parameters.push_back({"drive", drive});
    parameters.push_back({"tone", tone});
    parameters.push_back({"gate_enabled", gateEnabled});
    parameters.push_back({"gate_threshold", gateThreshold});
    return parameters;
  };

  std::vector<Preset> defaults;
  defaults.push_back(Preset{
    "factory-clean",
    "Factory Clean",
    "Factory",
    "Balanced clean tone with plenty of headroom.",
    "",
    "",
    {},
    {},
    presetParameters(0.0, 0.0, 0.15, 0.55, 0.0, -60.0),
  });

  defaults.push_back(Preset{
    "factory-breakup",
    "Edge of Breakup",
    "Factory",
    "Touch-sensitive crunch that works great with single coils.",
    "",
    "",
    {},
    {},
    presetParameters(-3.0, 0.0, 0.45, 0.5, 0.0, -55.0),
  });

  defaults.push_back(Preset{
    "factory-highgain",
    "Saturated Lead",
    "Factory",
    "Tight high-gain lead preset with a gentle noise gate.",
    "",
    "",
    {"noise_gate"},
    {},
    presetParameters(-6.0, -3.0, 0.85, 0.65, 1.0, -50.0),
  });

  return defaults;
}

nlohmann::json SerializePreset(const Preset& preset)
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
    attachments.push_back({
      {"type", attachment.type},
      {"filePath", attachment.filePath.generic_string()},
      {"hash", attachment.hash},
    });
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

nlohmann::json SerializeAllPresets(const std::vector<Preset>& presets)
{
  nlohmann::json jsonRoot;
  jsonRoot["presets"] = nlohmann::json::array();
  for (const auto& preset : presets)
  {
    jsonRoot["presets"].push_back(SerializePreset(preset));
  }

  return jsonRoot;
}

Preset DeserializePreset(const nlohmann::json& jsonPreset)
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
      attachment.filePath = jsonAttachment.value("filePath", "");
      preset.attachments.push_back(std::move(attachment));
    }
  }

  if (jsonPreset.contains("parameters") && jsonPreset["parameters"].is_array())
  {
    for (const auto& jsonParameter : jsonPreset["parameters"])
    {
      PresetParameter parameter;
      parameter.id = jsonParameter.value("id", "");
      parameter.value = jsonParameter.value("value", 0.0);
      preset.parameters.push_back(parameter);
    }
  }

  return preset;
}

std::vector<Preset> DeserializeAllPresets(const std::string& serialized)
{
  std::vector<Preset> presets;
  if (serialized.empty())
  {
    return presets;
  }

  nlohmann::json jsonRoot = nlohmann::json::parse(serialized, nullptr, false);
  if (jsonRoot.is_discarded())
  {
    return presets;
  }

  if (!jsonRoot.contains("presets") || !jsonRoot["presets"].is_array())
  {
    return presets;
  }

  for (const auto& jsonPreset : jsonRoot["presets"])
  {
    presets.push_back(DeserializePreset(jsonPreset));
  }

  return presets;
}

} // namespace

PresetStorage::PresetStorage()
{
  const auto presetDir = mFileSystem.EnsureDirectory(mFileSystem.ResolvePresetDirectory());
  if (presetDir)
  {
    mPresetFile = *presetDir / "local_presets.json";
  }

  LoadFromDisk();
  EnsureDefaultPresets();
}

PresetStorage::~PresetStorage() = default;

bool PresetStorage::Serialize(iplug::IByteChunk& chunk) const
{
  const nlohmann::json serialized = SerializeAllPresets(mPresets);
  const std::string payload = serialized.dump();
  const uint32_t size = static_cast<uint32_t>(payload.size());
  chunk.PutBytes(&size, sizeof(size));
  chunk.PutBytes(payload.data(), static_cast<int>(payload.size()));
  return true;
}

int PresetStorage::Unserialize(const iplug::IByteChunk& chunk, int startPos)
{
  uint32_t size = 0;
  int position = chunk.GetBytes(&size, sizeof(size), startPos);
  if (position < 0 || size == 0)
  {
    return startPos;
  }

  std::string serialized(size, '\0');
  position = chunk.GetBytes(serialized.data(), static_cast<int>(size), position);
  if (position < 0)
  {
    return startPos;
  }

  mPresets = DeserializeAllPresets(serialized);
  PersistToDisk();

  return position;
}

void PresetStorage::SavePreset(const Preset& preset)
{
  Preset presetCopy = preset;
  for (auto& attachment : presetCopy.attachments)
  {
    if (attachment.hash.empty() && !attachment.filePath.empty())
    {
      attachment.hash = mHasher.HashFile(attachment.filePath);
    }
  }

  auto it = std::find_if(mPresets.begin(), mPresets.end(), [&](const Preset& candidate) {
    return candidate.id == presetCopy.id;
  });

  if (it != mPresets.end())
  {
    *it = std::move(presetCopy);
  }
  else
  {
    mPresets.push_back(std::move(presetCopy));
  }

  PersistToDisk();
}

std::vector<Preset> PresetStorage::ListPresets() const
{
  return mPresets;
}

std::optional<Preset> PresetStorage::FindPreset(const std::string& id) const
{
  auto it = std::find_if(mPresets.begin(), mPresets.end(), [&](const Preset& candidate) {
    return candidate.id == id;
  });

  if (it == mPresets.end())
  {
    return std::nullopt;
  }

  return *it;
}

void PresetStorage::PersistToDisk() const
{
  if (mPresetFile.empty())
  {
    return;
  }

  std::ofstream output(mPresetFile, std::ios::binary | std::ios::trunc);
  if (!output)
  {
    return;
  }

  output << SerializeAllPresets(mPresets).dump(2);
}

void PresetStorage::LoadFromDisk()
{
  if (mPresetFile.empty() || !std::filesystem::exists(mPresetFile))
  {
    return;
  }

  std::ifstream input(mPresetFile, std::ios::binary);
  if (!input)
  {
    return;
  }

  std::string serialized((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  mPresets = DeserializeAllPresets(serialized);
}

void PresetStorage::EnsureDefaultPresets()
{
  const auto defaults = BuildDefaultPresets();
  bool changed = false;

  for (const auto& preset : defaults)
  {
    const auto existing = std::find_if(mPresets.begin(), mPresets.end(), [&](const Preset& candidate) {
      return candidate.id == preset.id;
    });

    if (existing == mPresets.end())
    {
      mPresets.push_back(preset);
      changed = true;
    }
  }

  if (changed)
  {
    PersistToDisk();
  }
}

} // namespace namguitar
