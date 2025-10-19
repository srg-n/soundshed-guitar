#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "config.h"
#include "IPlug_include_in_plug_hdr.h"

namespace iplug
{
namespace igraphics
{
class IGraphics;
} // namespace igraphics
} // namespace iplug

namespace namguitar
{
class NAMDSPManager;
class PresetManager;
class WebUIBridge;
struct Preset;
class NAMGuitarPlugin final : public iplug::Plugin
{
public:
  explicit NAMGuitarPlugin(const iplug::InstanceInfo& info);

  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnIdle() override;
  bool SerializeState(iplug::IByteChunk& chunk) const override;
  int UnserializeState(const iplug::IByteChunk& chunk, int startPos) override;
  void OnParamChange(int paramIdx) override;

  enum ParameterId
  {
    kParamInputTrim = 0,
    kParamOutputTrim,
    kParamDrive,
    kParamTone,
    kParamGateEnabled,
    kParamGateThreshold,
    kParamCount
  };

private:
  void InitializeParameters();
  void HandleWebViewMessages();
  void InitializeGraphics(iplug::igraphics::IGraphics& graphics);
  void HandleUIMessage(const std::string& message);
  void HandlePresetSearch(const nlohmann::json& payload);
  void HandlePresetDownload(const nlohmann::json& payload);
  void HandlePresetLoad(const nlohmann::json& payload);
  void BroadcastState();
  void ApplyPreset(const namguitar::Preset& preset);
  void SaveCurrentStateToPreset(namguitar::Preset& preset) const;

  std::unique_ptr<NAMDSPManager> mDSP;
  std::unique_ptr<PresetManager> mPresets;
  std::unique_ptr<WebUIBridge> mWebUI;
  std::string mActivePresetId;
  std::string mActiveModelPath;
  std::string mActiveIRPath;
  std::string mRemoteApiBaseUrl = "http://localhost:8000";
  bool mPendingStateBroadcast = true;
};
} // namespace namguitar

using NAMGuitarPlugin = namguitar::NAMGuitarPlugin;
