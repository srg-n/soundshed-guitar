#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "dsp/NAMDSPManager.h"
#include "IPlugConstants.h"

// Force factory registration by referencing factory functions directly.
// This ensures the translation units with static factory::Helper registrations
// are not stripped by the linker.
#include "NAM/wavenet.h"
#include "NAM/lstm.h"
#include "NAM/convnet.h"

namespace
{
// Touch factory symbols to prevent dead-stripping
[[maybe_unused]] volatile auto force_wavenet = &nam::wavenet::Factory;
[[maybe_unused]] volatile auto force_lstm = &nam::lstm::Factory;
[[maybe_unused]] volatile auto force_convnet = &nam::convnet::Factory;
} // namespace

namespace fs = std::filesystem;

namespace
{
constexpr double kPi = 3.14159265358979323846;

nlohmann::json LoadJson(const fs::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input)
  {
    throw std::runtime_error("Failed to open JSON file: " + path.string());
  }

  nlohmann::json document;
  input >> document;
  return document;
}

struct LibraryEntry
{
  fs::path filePath;
  std::string title;
};

std::string Describe(const fs::path& path)
{
  fs::path preferred = path;
  preferred.make_preferred();
  return preferred.string();
}

// Generate a simple sine wave test signal
void GenerateSineWave(std::vector<iplug::sample>& buffer, double frequency, double sampleRate, double amplitude = 0.5)
{
  for (std::size_t i = 0; i < buffer.size(); ++i)
  {
    buffer[i] = static_cast<iplug::sample>(amplitude * std::sin(2.0 * kPi * frequency * static_cast<double>(i) / sampleRate));
  }
}

// Generate an impulse (click) signal - useful for testing IR convolution
void GenerateImpulse(std::vector<iplug::sample>& buffer, double amplitude = 0.8)
{
  std::fill(buffer.begin(), buffer.end(), static_cast<iplug::sample>(0));
  if (!buffer.empty())
  {
    buffer[0] = static_cast<iplug::sample>(amplitude);
  }
}

// Generate white noise
void GenerateNoise(std::vector<iplug::sample>& buffer, double amplitude = 0.3)
{
  for (auto& sample : buffer)
  {
    // Simple LCG random number generator
    static unsigned int seed = 12345;
    seed = seed * 1103515245 + 12345;
    double rand01 = static_cast<double>((seed >> 16) & 0x7FFF) / 32767.0;
    sample = static_cast<iplug::sample>(amplitude * (2.0 * rand01 - 1.0));
  }
}

struct SignalAnalysis
{
  bool hasNaN = false;
  bool hasInf = false;
  bool isAllZeros = true;
  bool isAllSameValue = true;
  double peakValue = 0.0;
  double rmsValue = 0.0;
  double dcOffset = 0.0;
};

SignalAnalysis AnalyzeSignal(const std::vector<iplug::sample>& buffer)
{
  SignalAnalysis result;
  
  if (buffer.empty())
  {
    return result;
  }

  double sumSquares = 0.0;
  double sum = 0.0;
  const iplug::sample firstValue = buffer[0];

  for (const auto& sample : buffer)
  {
    if (std::isnan(sample))
    {
      result.hasNaN = true;
    }
    if (std::isinf(sample))
    {
      result.hasInf = true;
    }
    
    const double absSample = std::abs(static_cast<double>(sample));
    if (absSample > result.peakValue)
    {
      result.peakValue = absSample;
    }
    
    if (sample != 0.0)
    {
      result.isAllZeros = false;
    }
    
    if (sample != firstValue)
    {
      result.isAllSameValue = false;
    }
    
    sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
    sum += static_cast<double>(sample);
  }

  result.rmsValue = std::sqrt(sumSquares / static_cast<double>(buffer.size()));
  result.dcOffset = sum / static_cast<double>(buffer.size());

  return result;
}

struct ProcessingTestResult
{
  bool success = false;
  std::string errorMessage;
  SignalAnalysis inputAnalysis;
  SignalAnalysis outputAnalysis;
};

ProcessingTestResult TestDSPProcessing(namguitar::NAMDSPManager& dsp, int blockSize, double sampleRate)
{
  ProcessingTestResult result;

  // Create stereo input/output buffers
  std::vector<iplug::sample> inputL(static_cast<std::size_t>(blockSize));
  std::vector<iplug::sample> inputR(static_cast<std::size_t>(blockSize));
  std::vector<iplug::sample> outputL(static_cast<std::size_t>(blockSize));
  std::vector<iplug::sample> outputR(static_cast<std::size_t>(blockSize));

  // Generate test signal - 440 Hz sine wave (A4 note)
  GenerateSineWave(inputL, 440.0, sampleRate, 0.5);
  GenerateSineWave(inputR, 440.0, sampleRate, 0.5);

  // Analyze input
  result.inputAnalysis = AnalyzeSignal(inputL);

  // Set up buffer pointers
  iplug::sample* inputs[2] = {inputL.data(), inputR.data()};
  iplug::sample* outputs[2] = {outputL.data(), outputR.data()};

  // Process audio through DSP
  try
  {
    dsp.Process(inputs, outputs, blockSize);
  }
  catch (const std::exception& ex)
  {
    result.errorMessage = std::string("DSP processing threw exception: ") + ex.what();
    return result;
  }
  catch (...)
  {
    result.errorMessage = "DSP processing threw unknown exception";
    return result;
  }

  // Analyze output
  result.outputAnalysis = AnalyzeSignal(outputL);

  // Validate output
  if (result.outputAnalysis.hasNaN)
  {
    result.errorMessage = "Output contains NaN values";
    return result;
  }

  if (result.outputAnalysis.hasInf)
  {
    result.errorMessage = "Output contains infinite values";
    return result;
  }

  if (result.outputAnalysis.isAllZeros)
  {
    result.errorMessage = "Output is all zeros (no signal produced)";
    return result;
  }

  if (result.outputAnalysis.isAllSameValue)
  {
    result.errorMessage = "Output is all the same value (DC signal)";
    return result;
  }

  // Check for reasonable output levels (not clipping excessively)
  if (result.outputAnalysis.peakValue > 10.0)
  {
    result.errorMessage = "Output peak level is excessively high (" + 
                          std::to_string(result.outputAnalysis.peakValue) + ")";
    return result;
  }

  result.success = true;
  return result;
}

// Process multiple blocks to test stability over time
ProcessingTestResult TestDSPStability(namguitar::NAMDSPManager& dsp, int blockSize, double sampleRate, int numBlocks)
{
  ProcessingTestResult result;

  std::vector<iplug::sample> inputL(static_cast<std::size_t>(blockSize));
  std::vector<iplug::sample> inputR(static_cast<std::size_t>(blockSize));
  std::vector<iplug::sample> outputL(static_cast<std::size_t>(blockSize));
  std::vector<iplug::sample> outputR(static_cast<std::size_t>(blockSize));

  iplug::sample* inputs[2] = {inputL.data(), inputR.data()};
  iplug::sample* outputs[2] = {outputL.data(), outputR.data()};

  for (int block = 0; block < numBlocks; ++block)
  {
    // Vary the test signal slightly between blocks
    const double frequency = 220.0 + static_cast<double>(block) * 20.0; // Vary from 220Hz to 420Hz
    GenerateSineWave(inputL, frequency, sampleRate, 0.4);
    GenerateSineWave(inputR, frequency, sampleRate, 0.4);

    try
    {
      dsp.Process(inputs, outputs, blockSize);
    }
    catch (const std::exception& ex)
    {
      result.errorMessage = "Block " + std::to_string(block) + " threw exception: " + ex.what();
      return result;
    }

    // Check output on each block
    const auto analysis = AnalyzeSignal(outputL);
    
    if (analysis.hasNaN)
    {
      result.errorMessage = "Block " + std::to_string(block) + " produced NaN";
      return result;
    }
    
    if (analysis.hasInf)
    {
      result.errorMessage = "Block " + std::to_string(block) + " produced infinity";
      return result;
    }

    if (analysis.peakValue > 10.0)
    {
      result.errorMessage = "Block " + std::to_string(block) + " has excessive peak: " + 
                            std::to_string(analysis.peakValue);
      return result;
    }
  }

  result.success = true;
  result.outputAnalysis = AnalyzeSignal(outputL); // Final block analysis
  return result;
}

} // namespace

int main()
{
#ifndef NAMGUITAR_TEST_RESOURCES_DIR
#error "NAMGUITAR_TEST_RESOURCES_DIR must be defined"
#endif
  try
  {
    const fs::path resourcesDir = fs::path(NAMGUITAR_TEST_RESOURCES_DIR);
    const fs::path dataDir = resourcesDir / "ui" / "data";

    std::vector<std::string> errors;
    const auto recordError = [&errors](std::string message) {
      errors.push_back(std::move(message));
    };

    // Load model library
    const auto audioModelsJson = LoadJson(dataDir / "audiofx-models.json");
    std::unordered_map<std::string, LibraryEntry> modelLibrary;
    if (audioModelsJson.is_array())
    {
      for (const auto& entry : audioModelsJson)
      {
        const std::string id = entry.value("id", "");
        const std::string relPath = entry.value("filePath", "");
        if (!id.empty() && !relPath.empty())
        {
          modelLibrary.emplace(id, LibraryEntry{resourcesDir / relPath, entry.value("title", id)});
        }
      }
    }

    // Load IR library
    const auto irLibraryJson = LoadJson(dataDir / "ir-library.json");
    std::unordered_map<std::string, LibraryEntry> irLibrary;
    if (irLibraryJson.is_array())
    {
      for (const auto& entry : irLibraryJson)
      {
        const std::string id = entry.value("id", "");
        const std::string relPath = entry.value("filePath", "");
        if (!id.empty() && !relPath.empty())
        {
          irLibrary.emplace(id, LibraryEntry{resourcesDir / relPath, entry.value("title", id)});
        }
      }
    }

    // Load and test each preset
    const auto presetsJson = LoadJson(dataDir / "default-presets.json");
    if (!presetsJson.is_array())
    {
      throw std::runtime_error("default-presets.json is not an array");
    }

    constexpr double kTestSampleRate = 48000.0;
    constexpr int kTestBlockSize = 512;
    constexpr int kStabilityBlocks = 10; // Process 10 blocks for stability test

    int presetsProcessed = 0;
    int presetsTested = 0;

    std::cout << "DSP Processing Test - Testing audio processing for each preset\n";
    std::cout << "================================================================\n\n";

    for (const auto& preset : presetsJson)
    {
      const std::string presetId = preset.value("id", "<unnamed>");
      const std::string presetName = preset.value("name", presetId);
      const std::string modelId = preset.value("audioFxModelId", "");
      const std::string irId = preset.value("irId", "");

      ++presetsTested;
      std::cout << "Testing: " << presetName << "... ";

      // Skip if model or IR not found in library
      if (!modelLibrary.contains(modelId))
      {
        recordError("Preset '" + presetName + "' references unknown model: " + modelId);
        std::cout << "SKIP (unknown model)\n";
        continue;
      }
      if (!irLibrary.contains(irId))
      {
        recordError("Preset '" + presetName + "' references unknown IR: " + irId);
        std::cout << "SKIP (unknown IR)\n";
        continue;
      }

      const fs::path modelPath = modelLibrary.at(modelId).filePath;
      const fs::path irPath = irLibrary.at(irId).filePath;

      // Create a fresh DSP manager for each preset
      namguitar::NAMDSPManager dsp;
      dsp.Prepare(kTestSampleRate, kTestBlockSize);

      // Load model
      if (!dsp.LoadModel(modelPath))
      {
        recordError("Preset '" + presetName + "': failed to load model");
        std::cout << "FAIL (model load)\n";
        continue;
      }

      // Load IR
      if (!dsp.LoadImpulseResponse(irPath))
      {
        recordError("Preset '" + presetName + "': failed to load IR");
        std::cout << "FAIL (IR load)\n";
        continue;
      }

      // Apply some preset parameters (use defaults if not specified)
      dsp.SetInputTrim(preset.value("inputTrimDb", 0.0));
      dsp.SetOutputTrim(preset.value("outputTrimDb", 0.0));
      dsp.SetDrive(preset.value("driveAmount", 1.0));
      dsp.SetTone(preset.value("toneTilt", 0.0));
      dsp.SetMix(preset.value("mix", 1.0));
      dsp.SetGateEnabled(preset.value("gateEnabled", false));
      dsp.SetGateThreshold(preset.value("gateThreshold", -60.0));

      // Test 1: Basic processing test
      auto basicResult = TestDSPProcessing(dsp, kTestBlockSize, kTestSampleRate);
      if (!basicResult.success)
      {
        recordError("Preset '" + presetName + "' basic processing: " + basicResult.errorMessage);
        std::cout << "FAIL (" << basicResult.errorMessage << ")\n";
        continue;
      }

      // Test 2: Stability test (process multiple blocks)
      auto stabilityResult = TestDSPStability(dsp, kTestBlockSize, kTestSampleRate, kStabilityBlocks);
      if (!stabilityResult.success)
      {
        recordError("Preset '" + presetName + "' stability: " + stabilityResult.errorMessage);
        std::cout << "FAIL (" << stabilityResult.errorMessage << ")\n";
        continue;
      }

      ++presetsProcessed;
      std::cout << "OK (peak=" << std::fixed << std::setprecision(3) 
                << basicResult.outputAnalysis.peakValue 
                << ", rms=" << basicResult.outputAnalysis.rmsValue << ")\n";
    }

    std::cout << "\n================================================================\n";
    std::cout << "DSP Processing Results: " << presetsProcessed << "/" << presetsTested 
              << " presets processed successfully.\n";

    if (!errors.empty())
    {
      std::cerr << "\nDSP processing test failed with " << errors.size() << " issue(s):\n";
      for (const auto& error : errors)
      {
        std::cerr << " - " << error << '\n';
      }
      return 1;
    }

    std::cout << "\nAll default presets processed audio successfully." << std::endl;
    return 0;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "DSP processing test encountered a fatal error: " << ex.what() << std::endl;
    return 1;
  }
}
