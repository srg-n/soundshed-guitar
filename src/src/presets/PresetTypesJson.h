#pragma once

#include "presets/PresetTypes.h"
#include <nlohmann/json.hpp>

namespace guitarfx
{
  // JSON serialization for GlobalSignalChainConfig

  inline void to_json(nlohmann::json& j, const GlobalSignalChainConfig::PreChain& p)
  {
    j = nlohmann::json{
      {"gateEnabled", p.gateEnabled},
      {"gateThreshold", p.gateThreshold},
      {"gateAttack", p.gateAttack},
      {"gateHold", p.gateHold},
      {"gateRelease", p.gateRelease},
      {"transposeEnabled", p.transposeEnabled},
      {"transposeSemitones", p.transposeSemitones}
    };
  }

  inline void from_json(const nlohmann::json& j, GlobalSignalChainConfig::PreChain& p)
  {
    p.gateEnabled = j.value("gateEnabled", false);
    p.gateThreshold = j.value("gateThreshold", -40.0);
    p.gateAttack = j.value("gateAttack", 0.5);
    p.gateHold = j.value("gateHold", 50.0);
    p.gateRelease = j.value("gateRelease", 100.0);
    p.transposeEnabled = j.value("transposeEnabled", false);
    p.transposeSemitones = j.value("transposeSemitones", 0);
  }

  inline void to_json(nlohmann::json& j, const GlobalSignalChainConfig::PostChain& p)
  {
    j = nlohmann::json{
      {"eqEnabled", p.eqEnabled},
      {"eqLowGain", p.eqLowGain},
      {"eqLowFreq", p.eqLowFreq},
      {"eqLowMidGain", p.eqLowMidGain},
      {"eqLowMidFreq", p.eqLowMidFreq},
      {"eqLowMidQ", p.eqLowMidQ},
      {"eqHighMidGain", p.eqHighMidGain},
      {"eqHighMidFreq", p.eqHighMidFreq},
      {"eqHighMidQ", p.eqHighMidQ},
      {"eqHighGain", p.eqHighGain},
      {"eqHighFreq", p.eqHighFreq},
      {"doublerEnabled", p.doublerEnabled},
      {"doublerDelay", p.doublerDelay},
      {"doublerMix", p.doublerMix},
      {"doublerDetune", p.doublerDetune}
    };
  }

  inline void from_json(const nlohmann::json& j, GlobalSignalChainConfig::PostChain& p)
  {
    p.eqEnabled = j.value("eqEnabled", false);
    p.eqLowGain = j.value("eqLowGain", 0.0);
    p.eqLowFreq = j.value("eqLowFreq", 100.0);
    p.eqLowMidGain = j.value("eqLowMidGain", 0.0);
    p.eqLowMidFreq = j.value("eqLowMidFreq", 400.0);
    p.eqLowMidQ = j.value("eqLowMidQ", 1.0);
    p.eqHighMidGain = j.value("eqHighMidGain", 0.0);
    p.eqHighMidFreq = j.value("eqHighMidFreq", 2000.0);
    p.eqHighMidQ = j.value("eqHighMidQ", 1.0);
    p.eqHighGain = j.value("eqHighGain", 0.0);
    p.eqHighFreq = j.value("eqHighFreq", 8000.0);
    p.doublerEnabled = j.value("doublerEnabled", false);
    p.doublerDelay = j.value("doublerDelay", 20.0);
    p.doublerMix = j.value("doublerMix", 0.5);
    p.doublerDetune = j.value("doublerDetune", 5.0);
  }

  inline void to_json(nlohmann::json& j, const GlobalSignalChainConfig& c)
  {
    j = nlohmann::json{
      {"inputGain", c.inputGain},
      {"monoMode", c.monoMode},
      {"inputChannel", c.inputChannel},
      {"autoLevelInput", c.autoLevelInput},
      {"outputGain", c.outputGain},
      {"autoLevelOutput", c.autoLevelOutput},
      {"limiterEnabled", c.limiterEnabled},
      {"preChain", c.preChain},
      {"postChain", c.postChain}
    };
  }

  inline void from_json(const nlohmann::json& j, GlobalSignalChainConfig& c)
  {
    c.inputGain = j.value("inputGain", 0.0);
    c.monoMode = j.value("monoMode", false);
    c.inputChannel = j.value("inputChannel", 0);
    c.autoLevelInput = j.value("autoLevelInput", false);
    c.outputGain = j.value("outputGain", 0.0);
    c.autoLevelOutput = j.value("autoLevelOutput", false);
    c.limiterEnabled = j.value("limiterEnabled", false);
    
    if (j.contains("preChain") && j["preChain"].is_object())
      c.preChain = j["preChain"].get<GlobalSignalChainConfig::PreChain>();
    
    if (j.contains("postChain") && j["postChain"].is_object())
      c.postChain = j["postChain"].get<GlobalSignalChainConfig::PostChain>();
  }

} // namespace guitarfx
