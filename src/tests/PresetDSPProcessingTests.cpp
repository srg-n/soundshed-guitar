/**
 * @file PresetDSPProcessingTests.cpp
 * @brief Integration tests for preset loading and DSP processing through the signal graph
 *
 * These tests verify the complete signal path:
 * 1. Preset JSON parsing → Preset struct
 * 2. GraphDSPManager loads preset and configures SignalGraphExecutor
 * 3. Resources (NAM models, IRs) are resolved and loaded into effect processors
 * 4. Audio flows through the signal graph in correct topological order
 * 5. Output audio is valid (no NaN/Inf, not silent, reasonable levels)
 */

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "dsp/GraphDSPManager.h"
#include "presets/PresetStorage.h"
#include "presets/PresetTypes.h"
#include "resources/ResourceLibrary.h"
#include "IPlugConstants.h"

namespace fs = std::filesystem;

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTestSampleRate = 48000.0;
constexpr int kTestBlockSize = 512;
constexpr int kStabilityBlocks = 10;

// ============================================================================
// Analysis Structures
// ============================================================================

struct NodeStageResult
{
  std::string stageName;
  double peakValue = 0.0;
  double rmsValue = 0.0;
  bool hasNaN = false;
  bool hasInf = false;
  bool isAllZeros = false;
};

// ============================================================================
// JSON Loading
// ============================================================================

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

// ============================================================================
// Signal Generation
// ============================================================================

void GenerateSineWave(std::vector<double>& buffer, double frequency, double sampleRate, double amplitude = 0.5)
{
  for (std::size_t i = 0; i < buffer.size(); ++i)
  {
    buffer[i] = amplitude * std::sin(2.0 * kPi * frequency * static_cast<double>(i) / sampleRate);
  }
}

void GenerateImpulse(std::vector<double>& buffer, double amplitude = 0.8)
{
  std::fill(buffer.begin(), buffer.end(), 0.0);
  if (!buffer.empty())
  {
    buffer[0] = amplitude;
  }
}

void GenerateNoise(std::vector<double>& buffer, double amplitude = 0.3)
{
  static unsigned int seed = 12345;
  for (auto& sample : buffer)
  {
    seed = seed * 1103515245 + 12345;
    double rand01 = static_cast<double>((seed >> 16) & 0x7FFF) / 32767.0;
    sample = amplitude * (2.0 * rand01 - 1.0);
  }
}

// ============================================================================
// Signal Analysis
// ============================================================================

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

SignalAnalysis AnalyzeSignal(const std::vector<double>& buffer)
{
  SignalAnalysis result;
  
  if (buffer.empty())
  {
    return result;
  }

  double sumSquares = 0.0;
  double sum = 0.0;
  const double firstValue = buffer[0];

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
    
    const double absSample = std::abs(sample);
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
    
    sumSquares += sample * sample;
    sum += sample;
  }

  result.rmsValue = std::sqrt(sumSquares / static_cast<double>(buffer.size()));
  result.dcOffset = sum / static_cast<double>(buffer.size());

  return result;
}

// ============================================================================
// Test Result Structures
// ============================================================================

struct ProcessingTestResult
{
  bool success = false;
  std::string errorMessage;
  SignalAnalysis inputAnalysis;
  SignalAnalysis outputAnalysis;
};

struct GraphValidationResult
{
  bool valid = false;
  std::string errorMessage;
  std::vector<std::string> executionOrder;
  bool hasInputNode = false;
  bool hasOutputNode = false;
  bool hasAmpNode = false;
  bool hasCabNode = false;
};

// ============================================================================
// Graph Validation
// ============================================================================

GraphValidationResult ValidatePresetGraph(const guitarfx::Preset& preset)
{
  GraphValidationResult result;
  
  // Check for required nodes
  for (const auto& node : preset.graph.nodes)
  {
    if (node.type == "input" || node.id == "__input__")
    {
      result.hasInputNode = true;
    }
    else if (node.type == "output" || node.id == "__output__")
    {
      result.hasOutputNode = true;
    }
    else if (node.type == "amp_nam")
    {
      result.hasAmpNode = true;
    }
    else if (node.type == "cab_ir")
    {
      result.hasCabNode = true;
    }
  }
  
  // Check edges reference valid nodes
  for (const auto& edge : preset.graph.edges)
  {
    bool fromValid = (edge.from == "__input__");
    bool toValid = (edge.to == "__output__");
    
    for (const auto& node : preset.graph.nodes)
    {
      if (node.id == edge.from)
        fromValid = true;
      if (node.id == edge.to)
        toValid = true;
    }
    
    if (!fromValid)
    {
      result.errorMessage = "Edge references unknown source node: " + edge.from;
      return result;
    }
    if (!toValid)
    {
      result.errorMessage = "Edge references unknown target node: " + edge.to;
      return result;
    }
  }
  
  if (!result.hasAmpNode)
  {
    result.errorMessage = "Preset has no amp_nam node";
    return result;
  }
  
  if (!result.hasCabNode)
  {
    result.errorMessage = "Preset has no cab_ir node";
    return result;
  }
  
  result.valid = true;
  return result;
}

// ============================================================================
// DSP Processing Tests
// ============================================================================

ProcessingTestResult TestGraphDSPProcessing(guitarfx::GraphDSPManager& dsp, int blockSize, double sampleRate)
{
  ProcessingTestResult result;

  // Create stereo input/output buffers
  std::vector<double> inputL(static_cast<std::size_t>(blockSize));
  std::vector<double> inputR(static_cast<std::size_t>(blockSize));
  std::vector<double> outputL(static_cast<std::size_t>(blockSize));
  std::vector<double> outputR(static_cast<std::size_t>(blockSize));

  // Generate test signal - 440 Hz sine wave (A4 note)
  GenerateSineWave(inputL, 440.0, sampleRate, 0.5);
  GenerateSineWave(inputR, 440.0, sampleRate, 0.5);

  // Analyze input
  result.inputAnalysis = AnalyzeSignal(inputL);

  // Set up buffer pointers
  double* inputs[2] = {inputL.data(), inputR.data()};
  double* outputs[2] = {outputL.data(), outputR.data()};

  // Process audio through DSP graph
  // NOTE: Process multiple blocks to account for FFT convolution latency in IR effects.
  // IR convolution using UPOLS algorithm has latency equal to the partition size (typically
  // equal to the block size). We process a few blocks to "warm up" the convolver.
  constexpr int kWarmupBlocks = 3;  // Enough to fill convolver delay line
  
  try
  {
    for (int i = 0; i < kWarmupBlocks; ++i)
    {
      dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), blockSize);
    }
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

  // Analyze output (from the last processed block, after warm-up)
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

  // Check for minimum audible output (at least -60 dB relative to input)
  const double minExpectedPeak = result.inputAnalysis.peakValue * 0.001; // -60 dB
  if (result.outputAnalysis.peakValue < minExpectedPeak)
  {
    result.errorMessage = "Output level too low (peak=" + 
                          std::to_string(result.outputAnalysis.peakValue) + 
                          ", expected at least " + std::to_string(minExpectedPeak) + ")";
    return result;
  }

  result.success = true;
  return result;
}

// Process multiple blocks to test stability over time
ProcessingTestResult TestGraphDSPStability(guitarfx::GraphDSPManager& dsp, int blockSize, double sampleRate, int numBlocks)
{
  ProcessingTestResult result;

  std::vector<double> inputL(static_cast<std::size_t>(blockSize));
  std::vector<double> inputR(static_cast<std::size_t>(blockSize));
  std::vector<double> outputL(static_cast<std::size_t>(blockSize));
  std::vector<double> outputR(static_cast<std::size_t>(blockSize));

  double* inputs[2] = {inputL.data(), inputR.data()};
  double* outputs[2] = {outputL.data(), outputR.data()};

  for (int block = 0; block < numBlocks; ++block)
  {
    // Vary the test signal slightly between blocks
    const double frequency = 220.0 + static_cast<double>(block) * 20.0; // 220Hz to 400Hz
    GenerateSineWave(inputL, frequency, sampleRate, 0.4);
    GenerateSineWave(inputR, frequency, sampleRate, 0.4);

    try
    {
      dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), blockSize);
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

// Test that switching presets properly resets DSP state
ProcessingTestResult TestPresetSwitching(guitarfx::GraphDSPManager& dsp, 
                                          const guitarfx::Preset& preset1,
                                          const guitarfx::Preset& preset2,
                                          int blockSize, double sampleRate)
{
  ProcessingTestResult result;

  std::vector<double> inputL(static_cast<std::size_t>(blockSize));
  std::vector<double> inputR(static_cast<std::size_t>(blockSize));
  std::vector<double> outputL(static_cast<std::size_t>(blockSize));
  std::vector<double> outputR(static_cast<std::size_t>(blockSize));

  GenerateSineWave(inputL, 440.0, sampleRate, 0.5);
  GenerateSineWave(inputR, 440.0, sampleRate, 0.5);

  double* inputs[2] = {inputL.data(), inputR.data()};
  double* outputs[2] = {outputL.data(), outputR.data()};

  // Process multiple blocks to account for FFT convolution latency
  constexpr int kWarmupBlocks = 3;

  // Load first preset
  if (!dsp.LoadPreset(preset1))
  {
    result.errorMessage = "Failed to load first preset";
    return result;
  }

  // Process with first preset (warm up the convolver)
  try
  {
    for (int i = 0; i < kWarmupBlocks; ++i)
    {
      dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), blockSize);
    }
  }
  catch (const std::exception& ex)
  {
    result.errorMessage = "Processing preset 1 failed: " + std::string(ex.what());
    return result;
  }

  auto analysis1 = AnalyzeSignal(outputL);
  
  // Reset and load second preset
  dsp.Reset();
  if (!dsp.LoadPreset(preset2))
  {
    result.errorMessage = "Failed to load second preset";
    return result;
  }

  // Process with second preset (warm up the new convolver)
  try
  {
    for (int i = 0; i < kWarmupBlocks; ++i)
    {
      dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), blockSize);
    }
  }
  catch (const std::exception& ex)
  {
    result.errorMessage = "Processing preset 2 failed: " + std::string(ex.what());
    return result;
  }

  auto analysis2 = AnalyzeSignal(outputL);

  // Verify both produced valid output
  if (analysis1.isAllZeros || analysis2.isAllZeros)
  {
    result.errorMessage = "One or both presets produced silent output";
    return result;
  }

  if (analysis1.hasNaN || analysis2.hasNaN)
  {
    result.errorMessage = "One or both presets produced NaN";
    return result;
  }

  result.success = true;
  result.outputAnalysis = analysis2;
  return result;
}

// ============================================================================
// Resource Validation
// ============================================================================

struct ResourceValidation
{
  bool valid = false;
  std::string errorMessage;
  std::string namResourceId;
  std::string irResourceId;
  fs::path namFilePath;
  fs::path irFilePath;
};

ResourceValidation ValidatePresetResources(const guitarfx::Preset& preset, 
                                            const nlohmann::json& modelsLibrary,
                                            const nlohmann::json& irLibrary,
                                            const fs::path& resourcesDir)
{
  ResourceValidation result;
  
  // Find NAM and IR resources from graph nodes
  for (const auto& node : preset.graph.nodes)
  {
    if (node.type == "amp_nam" && node.resource)
    {
      if (node.resource->resourceType == "nam" && !node.resource->resourceId.empty())
      {
        result.namResourceId = node.resource->resourceId;
      }
    }
    else if (node.type == "cab_ir" && node.resource)
    {
      if (node.resource->resourceType == "ir" && !node.resource->resourceId.empty())
      {
        result.irResourceId = node.resource->resourceId;
      }
    }
  }
  
  if (result.namResourceId.empty())
  {
    result.errorMessage = "No NAM resource found in amp_nam nodes";
    return result;
  }
  
  if (result.irResourceId.empty())
  {
    result.errorMessage = "No IR resource found in cab_ir nodes";
    return result;
  }
  
  // Look up NAM file path in library
  for (const auto& entry : modelsLibrary)
  {
    if (entry.value("id", "") == result.namResourceId)
    {
      result.namFilePath = resourcesDir / entry.value("filePath", "");
      break;
    }
  }
  
  // Look up IR file path in library
  for (const auto& entry : irLibrary)
  {
    if (entry.value("id", "") == result.irResourceId)
    {
      result.irFilePath = resourcesDir / entry.value("filePath", "");
      break;
    }
  }
  
  if (result.namFilePath.empty())
  {
    result.errorMessage = "NAM resource not found in library: " + result.namResourceId;
    return result;
  }
  
  if (result.irFilePath.empty())
  {
    result.errorMessage = "IR resource not found in library: " + result.irResourceId;
    return result;
  }
  
  if (!fs::exists(result.namFilePath))
  {
    result.errorMessage = "NAM file does not exist: " + result.namFilePath.string();
    return result;
  }
  
  if (!fs::exists(result.irFilePath))
  {
    result.errorMessage = "IR file does not exist: " + result.irFilePath.string();
    return result;
  }
  
  result.valid = true;
  return result;
}

// ============================================================================
// Simple NAM Test - Minimal test to verify NAM processing works
// ============================================================================

bool TestSimpleNAMProcessing(const fs::path& resourcesDir)
{
  std::cout << "\n========================================================================\n";
  std::cout << "Simple NAM Processing Test\n";
  std::cout << "========================================================================\n";

  // Create DSP manager
  guitarfx::GraphDSPManager dsp;
  
  // Populate resource library
  auto& library = dsp.GetResourceLibrary();
  guitarfx::LibraryResource libResource;
  libResource.type = "nam";
  libResource.id = "nam-jcm800-hi-g6";
  libResource.name = "JCM800 Hi Gain 6";
  libResource.filePath = resourcesDir / "amps" / "Guitar" / "TimR" / "JCM800 2203 1985" / "JCM800 Hi P6 B8 M4 T7 G6.nam";
  library.AddResource(libResource);
  
  std::cout << "Added resource to library: " << libResource.id << "\n";
  std::cout << "  Path: " << libResource.filePath << "\n";
  std::cout << "  Exists: " << (fs::exists(libResource.filePath) ? "YES" : "NO") << "\n";
  
  dsp.Prepare(kTestSampleRate, kTestBlockSize);

  // Build a minimal signal graph: input -> NAM -> output
  guitarfx::SignalGraph graph;
  
  guitarfx::GraphNode namNode;
  namNode.id = "nam_test";
  namNode.type = "amp_nam";
  namNode.category = "amp";
  namNode.enabled = true;
  namNode.params["inputGain"] = 0.0;
  namNode.params["outputGain"] = 0.0;
  
  // Use a library reference (same as presets do)
  guitarfx::ResourceRef resource;
  resource.resourceType = "nam";
  resource.resourceId = "nam-jcm800-hi-g6";
  namNode.resource = resource;
  
  std::cout << "Using library reference: type=" << resource.resourceType << ", id=" << resource.resourceId << "\n";
  
  graph.nodes.push_back(namNode);
  
  // Define edges: input -> nam -> output
  guitarfx::GraphEdge edge1;
  edge1.from = "__input__";
  edge1.to = "nam_test";
  edge1.gain = 1.0;
  graph.edges.push_back(edge1);
  
  guitarfx::GraphEdge edge2;
  edge2.from = "nam_test";
  edge2.to = "__output__";
  edge2.gain = 1.0;
  graph.edges.push_back(edge2);

  // Create preset with this graph
  guitarfx::Preset preset;
  preset.name = "Simple NAM Test";
  preset.graph = graph;
  preset.global.inputTrim = 0.0;
  preset.global.outputTrim = 0.0;
  preset.global.outputVolume = 1.0;

  std::cout << "Loading preset with library reference...\n";
  
  if (!dsp.LoadPreset(preset))
  {
    std::cout << "FAIL: LoadPreset failed\n";
    return false;
  }

  // Generate test signal (440Hz sine wave)
  std::vector<double> inputL(kTestBlockSize);
  std::vector<double> inputR(kTestBlockSize);
  std::vector<double> outputL(kTestBlockSize);
  std::vector<double> outputR(kTestBlockSize);

  GenerateSineWave(inputL, 440.0, kTestSampleRate, 0.5);
  GenerateSineWave(inputR, 440.0, kTestSampleRate, 0.5);

  double* inputs[2] = { inputL.data(), inputR.data() };
  double* outputs[2] = { outputL.data(), outputR.data() };

  // Process audio
  dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), kTestBlockSize);

  // Analyze output
  auto analysis = AnalyzeSignal(outputL);

  std::cout << "Output Analysis:\n";
  std::cout << "  Peak: " << analysis.peakValue << "\n";
  std::cout << "  RMS: " << analysis.rmsValue << "\n";
  std::cout << "  Has NaN: " << (analysis.hasNaN ? "YES" : "NO") << "\n";
  std::cout << "  Has Inf: " << (analysis.hasInf ? "YES" : "NO") << "\n";
  std::cout << "  All Zeros: " << (analysis.isAllZeros ? "YES" : "NO") << "\n";

  // Validate output
  if (analysis.hasNaN)
  {
    std::cout << "FAIL: Output contains NaN\n";
    return false;
  }

  if (analysis.hasInf)
  {
    std::cout << "FAIL: Output contains Inf\n";
    return false;
  }

  if (analysis.peakValue > 100.0)
  {
    std::cout << "FAIL: Output peak too high: " << analysis.peakValue << "\n";
    return false;
  }

  if (analysis.isAllZeros)
  {
    std::cout << "FAIL: Output is all zeros\n";
    return false;
  }

  std::cout << "PASS: NAM processing produced valid output\n";
  return true;
}

// ============================================================================
// Simple NAM+IR Test - Test basic NAM + IR combination
// ============================================================================

bool TestSimpleNAMWithIR(const fs::path& resourcesDir)
{
  std::cout << "\n========================================================================\n";
  std::cout << "Simple NAM+IR Processing Test\n";
  std::cout << "========================================================================\n";

  // Create DSP manager
  guitarfx::GraphDSPManager dsp;
  
  // Populate resource library with one NAM and one IR
  auto& library = dsp.GetResourceLibrary();
  
  guitarfx::LibraryResource namResource;
  namResource.type = "nam";
  namResource.id = "nam-fender-twin-clean";
  namResource.name = "Fender Twin Clean";
  namResource.filePath = resourcesDir / "amps" / "Guitar" / "Neil" / "FENDER Twin Reverb (Model 140) Clean 1989" / "FENDER Twin Reverb Clean PUSHED_1_1986.nam";
  library.AddResource(namResource);
  
  guitarfx::LibraryResource irResource;
  irResource.type = "ir";
  irResource.id = "ir-devils-lab-112jensen";
  irResource.name = "Jensen Cab";
  irResource.filePath = resourcesDir / "cabs" / "1x12" / "Jensen" / "Jensen P10R 15ohm-192khz.wav";
  library.AddResource(irResource);
  
  dsp.Prepare(kTestSampleRate, kTestBlockSize);

  // Build a minimal signal graph: input -> NAM -> IR -> output
  guitarfx::SignalGraph graph;
  
  guitarfx::GraphNode namNode;
  namNode.id = "amp_1";
  namNode.type = "amp_nam";
  namNode.category = "amp";
  namNode.enabled = true;
  namNode.params["inputGain"] = 0.0;
  namNode.params["outputGain"] = 0.0;
  guitarfx::ResourceRef namRef;
  namRef.resourceType = "nam";
  namRef.resourceId = "nam-fender-twin-clean";
  namNode.resource = namRef;
  graph.nodes.push_back(namNode);
  
  guitarfx::GraphNode irNode;
  irNode.id = "cab_1";
  irNode.type = "cab_ir";
  irNode.category = "cab";
  irNode.enabled = true;
  irNode.params["mix"] = 1.0;
  irNode.params["outputGain"] = 0.0;
  guitarfx::ResourceRef irRef;
  irRef.resourceType = "ir";
  irRef.resourceId = "ir-devils-lab-112jensen";
  irNode.resource = irRef;
  graph.nodes.push_back(irNode);
  
  // Define edges
  guitarfx::GraphEdge edge1;
  edge1.from = "__input__";
  edge1.to = "amp_1";
  edge1.gain = 1.0;
  graph.edges.push_back(edge1);
  
  guitarfx::GraphEdge edge2;
  edge2.from = "amp_1";
  edge2.to = "cab_1";
  edge2.gain = 1.0;
  graph.edges.push_back(edge2);
  
  guitarfx::GraphEdge edge3;
  edge3.from = "cab_1";
  edge3.to = "__output__";
  edge3.gain = 1.0;
  graph.edges.push_back(edge3);

  // Create preset
  guitarfx::Preset preset;
  preset.name = "Simple NAM+IR Test";
  preset.graph = graph;
  preset.global.inputTrim = 0.0;
  preset.global.outputTrim = 0.0;
  preset.global.outputVolume = 0.8;

  std::cout << "Loading NAM+IR preset...\n";
  
  if (!dsp.LoadPreset(preset))
  {
    std::cout << "FAIL: LoadPreset failed\n";
    return false;
  }

  // Generate test signal
  std::vector<double> inputL(kTestBlockSize);
  std::vector<double> inputR(kTestBlockSize);
  std::vector<double> outputL(kTestBlockSize);
  std::vector<double> outputR(kTestBlockSize);

  GenerateSineWave(inputL, 440.0, kTestSampleRate, 0.5);
  GenerateSineWave(inputR, 440.0, kTestSampleRate, 0.5);

  double* inputs[2] = { inputL.data(), inputR.data() };
  double* outputs[2] = { outputL.data(), outputR.data() };

  // Process with warmup blocks for IR convolver
  for (int i = 0; i < 3; ++i)
  {
    dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), kTestBlockSize);
  }

  // Analyze output
  auto analysis = AnalyzeSignal(outputL);

  std::cout << "Output Analysis:\n";
  std::cout << "  Peak: " << analysis.peakValue << "\n";
  std::cout << "  RMS: " << analysis.rmsValue << "\n";
  std::cout << "  Has NaN: " << (analysis.hasNaN ? "YES" : "NO") << "\n";
  std::cout << "  Has Inf: " << (analysis.hasInf ? "YES" : "NO") << "\n";
  std::cout << "  All Zeros: " << (analysis.isAllZeros ? "YES" : "NO") << "\n";

  // Validate output
  if (analysis.hasNaN)
  {
    std::cout << "FAIL: Output contains NaN\n";
    return false;
  }

  if (analysis.hasInf)
  {
    std::cout << "FAIL: Output contains Inf\n";
    return false;
  }

  if (analysis.peakValue > 100.0)
  {
    std::cout << "FAIL: Output peak too high: " << analysis.peakValue << "\n";
    return false;
  }

  if (analysis.isAllZeros)
  {
    std::cout << "FAIL: Output is all zeros\n";
    return false;
  }

  std::cout << "PASS: NAM+IR test produced valid output\n";
  return true;
}

// ============================================================================
// Progressive Node Test - Enable nodes one at a time to isolate issues
// ============================================================================

bool TestProgressiveNodeEnabling(const fs::path& resourcesDir)
{
  std::cout << "\n========================================================================\n";
  std::cout << "Progressive Node Enabling Test\n";
  std::cout << "========================================================================\n";
  std::cout << "This test progressively enables nodes to isolate signal corruption.\n\n";

  std::vector<NodeStageResult> results;

  // Test 1: Passthrough only (no processing nodes)
  {
    std::cout << "Stage 1: Input -> Output (passthrough)\n";
    
    guitarfx::GraphDSPManager dsp;
    dsp.Prepare(kTestSampleRate, kTestBlockSize);

    guitarfx::SignalGraph graph;
    
    // Only input->output edges, no processing nodes
    guitarfx::GraphEdge edge;
    edge.from = "__input__";
    edge.to = "__output__";
    edge.gain = 1.0;
    graph.edges.push_back(edge);

    guitarfx::Preset preset;
    preset.name = "Passthrough";
    preset.graph = graph;
    preset.global.inputTrim = 0.0;
    preset.global.outputTrim = 0.0;
    preset.global.outputVolume = 1.0;

    if (!dsp.LoadPreset(preset))
    {
      std::cout << "  FAIL: LoadPreset failed\n";
      return false;
    }

    std::vector<double> inputL(kTestBlockSize);
    std::vector<double> inputR(kTestBlockSize);
    std::vector<double> outputL(kTestBlockSize);
    std::vector<double> outputR(kTestBlockSize);

    GenerateSineWave(inputL, 440.0, kTestSampleRate, 0.5);
    GenerateSineWave(inputR, 440.0, kTestSampleRate, 0.5);

    double* inputs[2] = { inputL.data(), inputR.data() };
    double* outputs[2] = { outputL.data(), outputR.data() };

    dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), kTestBlockSize);
    auto analysis = AnalyzeSignal(outputL);

    NodeStageResult result;
    result.stageName = "Passthrough";
    result.peakValue = analysis.peakValue;
    result.rmsValue = analysis.rmsValue;
    result.hasNaN = analysis.hasNaN;
    result.hasInf = analysis.hasInf;
    result.isAllZeros = analysis.isAllZeros;
    results.push_back(result);

    std::cout << "  Peak: " << analysis.peakValue << ", RMS: " << analysis.rmsValue 
              << ", NaN: " << (analysis.hasNaN ? "Y" : "N") << "\n";
  }

  // Test 2: With NAM only
  {
    std::cout << "Stage 2: Input -> NAM -> Output\n";
    
    guitarfx::GraphDSPManager dsp;
    auto& library = dsp.GetResourceLibrary();
    
    guitarfx::LibraryResource namResource;
    namResource.type = "nam";
    namResource.id = "nam-jcm800-hi-g6";
    namResource.name = "JCM800";
    namResource.filePath = resourcesDir / "amps" / "Guitar" / "TimR" / "JCM800 2203 1985" / "JCM800 Hi P6 B8 M4 T7 G6.nam";
    library.AddResource(namResource);
    
    dsp.Prepare(kTestSampleRate, kTestBlockSize);

    guitarfx::SignalGraph graph;
    
    guitarfx::GraphNode namNode;
    namNode.id = "amp_1";
    namNode.type = "amp_nam";
    namNode.enabled = true;
    namNode.params["inputGain"] = 0.0;
    namNode.params["outputGain"] = 0.0;
    guitarfx::ResourceRef namRef;
    namRef.resourceType = "nam";
    namRef.resourceId = "nam-jcm800-hi-g6";
    namNode.resource = namRef;
    graph.nodes.push_back(namNode);
    
    guitarfx::GraphEdge edge1;
    edge1.from = "__input__";
    edge1.to = "amp_1";
    edge1.gain = 1.0;
    graph.edges.push_back(edge1);
    
    guitarfx::GraphEdge edge2;
    edge2.from = "amp_1";
    edge2.to = "__output__";
    edge2.gain = 1.0;
    graph.edges.push_back(edge2);

    guitarfx::Preset preset;
    preset.name = "NAM Only";
    preset.graph = graph;
    preset.global.inputTrim = 0.0;
    preset.global.outputTrim = 0.0;
    preset.global.outputVolume = 1.0;

    if (!dsp.LoadPreset(preset))
    {
      std::cout << "  FAIL: LoadPreset failed\n";
      return false;
    }

    std::vector<double> inputL(kTestBlockSize);
    std::vector<double> inputR(kTestBlockSize);
    std::vector<double> outputL(kTestBlockSize);
    std::vector<double> outputR(kTestBlockSize);

    GenerateSineWave(inputL, 440.0, kTestSampleRate, 0.5);
    GenerateSineWave(inputR, 440.0, kTestSampleRate, 0.5);

    double* inputs[2] = { inputL.data(), inputR.data() };
    double* outputs[2] = { outputL.data(), outputR.data() };

    dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), kTestBlockSize);
    auto analysis = AnalyzeSignal(outputL);

    NodeStageResult result;
    result.stageName = "NAM Only";
    result.peakValue = analysis.peakValue;
    result.rmsValue = analysis.rmsValue;
    result.hasNaN = analysis.hasNaN;
    result.hasInf = analysis.hasInf;
    result.isAllZeros = analysis.isAllZeros;
    results.push_back(result);

    std::cout << "  Peak: " << analysis.peakValue << ", RMS: " << analysis.rmsValue 
              << ", NaN: " << (analysis.hasNaN ? "Y" : "N") << "\n";
  }

  // Test 3: With NAM + IR
  {
    std::cout << "Stage 3: Input -> NAM -> IR -> Output\n";
    
    guitarfx::GraphDSPManager dsp;
    auto& library = dsp.GetResourceLibrary();
    
    guitarfx::LibraryResource namResource;
    namResource.type = "nam";
    namResource.id = "nam-jcm800-hi-g6";
    namResource.name = "JCM800";
    namResource.filePath = resourcesDir / "amps" / "Guitar" / "TimR" / "JCM800 2203 1985" / "JCM800 Hi P6 B8 M4 T7 G6.nam";
    library.AddResource(namResource);
    
    guitarfx::LibraryResource irResource;
    irResource.type = "ir";
    irResource.id = "ir-devils-lab-412v30";
    irResource.name = "1960 V30";
    irResource.filePath = resourcesDir / "cabs" / "4x12" / "Marshall" / "Marshall 1960A" / "Marshall 1960 Celestion G12M Greenback V30 Stereo Blend-192khz.wav";
    library.AddResource(irResource);
    
    dsp.Prepare(kTestSampleRate, kTestBlockSize);

    guitarfx::SignalGraph graph;
    
    guitarfx::GraphNode namNode;
    namNode.id = "amp_1";
    namNode.type = "amp_nam";
    namNode.enabled = true;
    namNode.params["inputGain"] = 0.0;
    namNode.params["outputGain"] = 0.0;
    guitarfx::ResourceRef namRef;
    namRef.resourceType = "nam";
    namRef.resourceId = "nam-jcm800-hi-g6";
    namNode.resource = namRef;
    graph.nodes.push_back(namNode);
    
    guitarfx::GraphNode irNode;
    irNode.id = "cab_1";
    irNode.type = "cab_ir";
    irNode.enabled = true;
    irNode.params["mix"] = 1.0;
    irNode.params["outputGain"] = 0.0;
    guitarfx::ResourceRef irRef;
    irRef.resourceType = "ir";
    irRef.resourceId = "ir-devils-lab-412v30";
    irNode.resource = irRef;
    graph.nodes.push_back(irNode);
    
    guitarfx::GraphEdge edge1;
    edge1.from = "__input__";
    edge1.to = "amp_1";
    edge1.gain = 1.0;
    graph.edges.push_back(edge1);
    
    guitarfx::GraphEdge edge2;
    edge2.from = "amp_1";
    edge2.to = "cab_1";
    edge2.gain = 1.0;
    graph.edges.push_back(edge2);
    
    guitarfx::GraphEdge edge3;
    edge3.from = "cab_1";
    edge3.to = "__output__";
    edge3.gain = 1.0;
    graph.edges.push_back(edge3);

    guitarfx::Preset preset;
    preset.name = "NAM+IR";
    preset.graph = graph;
    preset.global.inputTrim = 0.0;
    preset.global.outputTrim = 0.0;
    preset.global.outputVolume = 1.0;

    if (!dsp.LoadPreset(preset))
    {
      std::cout << "  FAIL: LoadPreset failed\n";
      return false;
    }

    std::vector<double> inputL(kTestBlockSize);
    std::vector<double> inputR(kTestBlockSize);
    std::vector<double> outputL(kTestBlockSize);
    std::vector<double> outputR(kTestBlockSize);

    GenerateSineWave(inputL, 440.0, kTestSampleRate, 0.5);
    GenerateSineWave(inputR, 440.0, kTestSampleRate, 0.5);

    double* inputs[2] = { inputL.data(), inputR.data() };
    double* outputs[2] = { outputL.data(), outputR.data() };

    // Multiple blocks for IR warmup
    for (int i = 0; i < 3; ++i)
    {
      dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), kTestBlockSize);
    }
    auto analysis = AnalyzeSignal(outputL);

    NodeStageResult result;
    result.stageName = "NAM+IR";
    result.peakValue = analysis.peakValue;
    result.rmsValue = analysis.rmsValue;
    result.hasNaN = analysis.hasNaN;
    result.hasInf = analysis.hasInf;
    result.isAllZeros = analysis.isAllZeros;
    results.push_back(result);

    std::cout << "  Peak: " << analysis.peakValue << ", RMS: " << analysis.rmsValue 
              << ", NaN: " << (analysis.hasNaN ? "Y" : "N") << "\n";
  }

  // Test 4: With NAM + Gate + IR (to test dynamics nodes)
  {
    std::cout << "Stage 4: Input -> Gate -> NAM -> IR -> Output\n";
    
    guitarfx::GraphDSPManager dsp;
    auto& library = dsp.GetResourceLibrary();
    
    guitarfx::LibraryResource namResource;
    namResource.type = "nam";
    namResource.id = "nam-jcm800-hi-g6";
    namResource.name = "JCM800";
    namResource.filePath = resourcesDir / "amps" / "Guitar" / "TimR" / "JCM800 2203 1985" / "JCM800 Hi P6 B8 M4 T7 G6.nam";
    library.AddResource(namResource);
    
    guitarfx::LibraryResource irResource;
    irResource.type = "ir";
    irResource.id = "ir-devils-lab-412v30";
    irResource.name = "1960 V30";
    irResource.filePath = resourcesDir / "cabs" / "4x12" / "Marshall" / "Marshall 1960A" / "Marshall 1960 Celestion G12M Greenback V30 Stereo Blend-192khz.wav";
    library.AddResource(irResource);
    
    dsp.Prepare(kTestSampleRate, kTestBlockSize);

    guitarfx::SignalGraph graph;
    
    guitarfx::GraphNode gateNode;
    gateNode.id = "gate_1";
    gateNode.type = "dynamics_gate";
    gateNode.enabled = true;
    gateNode.params["threshold"] = -55.0;
    gateNode.params["attack"] = 0.1;
    gateNode.params["hold"] = 50.0;
    gateNode.params["release"] = 100.0;
    graph.nodes.push_back(gateNode);
    
    guitarfx::GraphNode namNode;
    namNode.id = "amp_1";
    namNode.type = "amp_nam";
    namNode.enabled = true;
    namNode.params["inputGain"] = 0.0;
    namNode.params["outputGain"] = 0.0;
    guitarfx::ResourceRef namRef;
    namRef.resourceType = "nam";
    namRef.resourceId = "nam-jcm800-hi-g6";
    namNode.resource = namRef;
    graph.nodes.push_back(namNode);
    
    guitarfx::GraphNode irNode;
    irNode.id = "cab_1";
    irNode.type = "cab_ir";
    irNode.enabled = true;
    irNode.params["mix"] = 1.0;
    irNode.params["outputGain"] = 0.0;
    guitarfx::ResourceRef irRef;
    irRef.resourceType = "ir";
    irRef.resourceId = "ir-devils-lab-412v30";
    irNode.resource = irRef;
    graph.nodes.push_back(irNode);
    
    guitarfx::GraphEdge edge1;
    edge1.from = "__input__";
    edge1.to = "gate_1";
    edge1.gain = 1.0;
    graph.edges.push_back(edge1);
    
    guitarfx::GraphEdge edge2;
    edge2.from = "gate_1";
    edge2.to = "amp_1";
    edge2.gain = 1.0;
    graph.edges.push_back(edge2);
    
    guitarfx::GraphEdge edge3;
    edge3.from = "amp_1";
    edge3.to = "cab_1";
    edge3.gain = 1.0;
    graph.edges.push_back(edge3);
    
    guitarfx::GraphEdge edge4;
    edge4.from = "cab_1";
    edge4.to = "__output__";
    edge4.gain = 1.0;
    graph.edges.push_back(edge4);

    guitarfx::Preset preset;
    preset.name = "Gate+NAM+IR";
    preset.graph = graph;
    preset.global.inputTrim = 0.0;
    preset.global.outputTrim = 0.0;
    preset.global.outputVolume = 1.0;

    if (!dsp.LoadPreset(preset))
    {
      std::cout << "  FAIL: LoadPreset failed\n";
      return false;
    }

    std::vector<double> inputL(kTestBlockSize);
    std::vector<double> inputR(kTestBlockSize);
    std::vector<double> outputL(kTestBlockSize);
    std::vector<double> outputR(kTestBlockSize);

    GenerateSineWave(inputL, 440.0, kTestSampleRate, 0.5);
    GenerateSineWave(inputR, 440.0, kTestSampleRate, 0.5);

    double* inputs[2] = { inputL.data(), inputR.data() };
    double* outputs[2] = { outputL.data(), outputR.data() };

    for (int i = 0; i < 3; ++i)
    {
      dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), kTestBlockSize);
    }
    auto analysis = AnalyzeSignal(outputL);

    NodeStageResult result;
    result.stageName = "Gate+NAM+IR";
    result.peakValue = analysis.peakValue;
    result.rmsValue = analysis.rmsValue;
    result.hasNaN = analysis.hasNaN;
    result.hasInf = analysis.hasInf;
    result.isAllZeros = analysis.isAllZeros;
    results.push_back(result);

    std::cout << "  Peak: " << analysis.peakValue << ", RMS: " << analysis.rmsValue 
              << ", NaN: " << (analysis.hasNaN ? "Y" : "N") << "\n";
  }

  // Print summary
  std::cout << "\n------------------------------------------------------------------------\n";
  std::cout << "Summary of Progressive Stages:\n";
  std::cout << "------------------------------------------------------------------------\n";
  std::cout << std::left << std::setw(20) << "Stage" 
            << std::setw(15) << "Peak" 
            << std::setw(15) << "RMS"
            << std::setw(8) << "NaN"
            << std::setw(8) << "Inf"
            << std::setw(10) << "AllZeros" << "\n";
  std::cout << std::string(76, '-') << "\n";
  
  for (const auto& result : results)
  {
    std::cout << std::left << std::setw(20) << result.stageName
              << std::setw(15) << std::fixed << std::setprecision(6) << result.peakValue
              << std::setw(15) << std::fixed << std::setprecision(6) << result.rmsValue
              << std::setw(8) << (result.hasNaN ? "YES" : "NO")
              << std::setw(8) << (result.hasInf ? "YES" : "NO")
              << std::setw(10) << (result.isAllZeros ? "YES" : "NO") << "\n";
  }

  // Check for any broken stages
  bool allGood = true;
  for (const auto& result : results)
  {
    if (result.hasNaN || result.hasInf || result.isAllZeros || result.peakValue > 100.0)
    {
      allGood = false;
      break;
    }
  }

  if (allGood)
  {
    std::cout << "\nPASS: All progressive stages produced valid output\n";
    return true;
  }
  else
  {
    std::cout << "\nFAIL: Some stages produced invalid output (see summary above)\n";
    return false;
  }
}

// ============================================================================
// Single Preset Isolation Test  
// ============================================================================

bool TestSinglePresetIsolation(const fs::path& resourcesDir)
{
  std::cout << "\n========================================================================\n";
  std::cout << "Single Preset Isolation Test - Test 'Pristine Clean' in isolation\n";
  std::cout << "========================================================================\n";

  const auto dataDir = resourcesDir / "ui" / "data";

  // Load JSON data
  const auto audioModelsJson = LoadJson(dataDir / "audiofx-models.json");
  const auto irLibraryJson = LoadJson(dataDir / "ir-library.json");
  const auto presetsJson = LoadJson(dataDir / "default-presets.json");

  if (!presetsJson.is_array() || presetsJson.empty())
  {
    std::cout << "FAIL: Could not load presets JSON\n";
    return false;
  }

  // Find "Pristine Clean" preset
  std::optional<nlohmann::json> pristinePresetJson;
  for (const auto& presetJson : presetsJson)
  {
    if (presetJson.value("name", "") == "Pristine Clean")
    {
      pristinePresetJson = presetJson;
      break;
    }
  }

  if (!pristinePresetJson)
  {
    std::cout << "FAIL: Could not find 'Pristine Clean' preset\n";
    return false;
  }

  // Parse preset
  auto presetOpt = guitarfx::PresetStorage::DeserializeFromJson(pristinePresetJson->dump());
  if (!presetOpt)
  {
    std::cout << "FAIL: Could not parse 'Pristine Clean' preset\n";
    return false;
  }

  std::cout << "Found preset: " << presetOpt->name << "\n\n";

  // Create fresh DSP manager
  guitarfx::GraphDSPManager dsp;
  auto& library = dsp.GetResourceLibrary();

  // Populate resource library
  std::cout << "Loading NAM models...\n";
  for (const auto& entry : audioModelsJson)
  {
    guitarfx::LibraryResource resource;
    resource.type = "nam";
    resource.id = entry.value("id", "");
    resource.name = entry.value("title", entry.value("name", resource.id));
    resource.category = entry.value("category", "");
    resource.description = entry.value("description", "");
    resource.filePath = resourcesDir / entry.value("filePath", "");
    
    if (!resource.id.empty())
    {
      library.AddResource(resource);
    }
  }

  std::cout << "Loading IR cabinets...\n";
  for (const auto& entry : irLibraryJson)
  {
    guitarfx::LibraryResource resource;
    resource.type = "ir";
    resource.id = entry.value("id", "");
    resource.name = entry.value("title", entry.value("name", resource.id));
    resource.category = entry.value("category", "");
    resource.description = entry.value("description", "");
    resource.filePath = resourcesDir / entry.value("filePath", "");
    
    if (!resource.id.empty())
    {
      library.AddResource(resource);
    }
  }

  std::cout << "Resource library: " << library.GetResourcesByType("nam").size() << " NAMs, "
            << library.GetResourcesByType("ir").size() << " IRs\n\n";

  dsp.Prepare(kTestSampleRate, kTestBlockSize);

  std::cout << "Loading preset into DSP...\n";
  if (!dsp.LoadPreset(*presetOpt))
  {
    std::cout << "FAIL: LoadPreset failed\n";
    return false;
  }

  // Generate test signal
  std::vector<double> inputL(kTestBlockSize);
  std::vector<double> inputR(kTestBlockSize);
  std::vector<double> outputL(kTestBlockSize);
  std::vector<double> outputR(kTestBlockSize);

  GenerateSineWave(inputL, 440.0, kTestSampleRate, 0.5);
  GenerateSineWave(inputR, 440.0, kTestSampleRate, 0.5);

  double* inputs[2] = { inputL.data(), inputR.data() };
  double* outputs[2] = { outputL.data(), outputR.data() };

  std::cout << "Processing audio...\n";
  dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), kTestBlockSize);

  auto analysis = AnalyzeSignal(outputL);

  std::cout << "\nOutput Analysis (1st block):\n";
  std::cout << "  Peak: " << std::scientific << analysis.peakValue << std::defaultfloat << "\n";
  std::cout << "  RMS: " << analysis.rmsValue << "\n";
  std::cout << "  Has NaN: " << (analysis.hasNaN ? "YES" : "NO") << "\n";
  std::cout << "  Has Inf: " << (analysis.hasInf ? "YES" : "NO") << "\n";
  std::cout << "  All Zeros: " << (analysis.isAllZeros ? "YES" : "NO") << "\n";

  if (analysis.hasNaN || analysis.hasInf || analysis.peakValue > 100.0)
  {
    std::cout << "\nFAIL: Invalid output on first block\n";
    return false;
  }

  // Process more blocks like the full test does
  // NOTE: NOT resetting between blocks to match the full test behavior
  std::cout << "\nProcessing " << kStabilityBlocks << " stability blocks (no reset between blocks)...\n";
  for (int i = 0; i < kStabilityBlocks; ++i)
  {
    // Clear output buffers before each process call (this is normal test setup)
    std::fill(outputL.begin(), outputL.end(), 0.0);
    std::fill(outputR.begin(), outputR.end(), 0.0);
    
    dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), kTestBlockSize);
    
    // Print peak after each block to see accumulation
    auto blockAnalysis = AnalyzeSignal(outputL);
    std::cout << "  Block " << (i+2) << ": Peak=" << std::scientific << blockAnalysis.peakValue 
              << std::defaultfloat << ", NaN=" << (blockAnalysis.hasNaN ? "Y" : "N")  
              << ", Inf=" << (blockAnalysis.hasInf ? "Y" : "N") << "\n";
    
    if (blockAnalysis.hasNaN || blockAnalysis.hasInf)
    {
      std::cout << "FAIL: Invalid output detected at block " << (i+2) << "\n";
      return false;
    }
  }

  auto analysisAfter = AnalyzeSignal(outputL);

  std::cout << "\nOutput Analysis (after " << kStabilityBlocks << " blocks):\n";
  std::cout << "  Peak: " << std::scientific << analysisAfter.peakValue << std::defaultfloat << "\n";
  std::cout << "  RMS: " << analysisAfter.rmsValue << "\n";
  std::cout << "  Has NaN: " << (analysisAfter.hasNaN ? "YES" : "NO") << "\n";
  std::cout << "  Has Inf: " << (analysisAfter.hasInf ? "YES" : "NO") << "\n";
  std::cout << "  All Zeros: " << (analysisAfter.isAllZeros ? "YES" : "NO") << "\n";

  if (analysisAfter.hasNaN || analysisAfter.hasInf || analysisAfter.peakValue > 100.0)
  {
    std::cout << "\nFAIL: Invalid output after stability blocks\n";
    return false;
  }

  std::cout << "\nPASS: Single preset isolation test passed\n";
  return true;
}

// Test to isolate Modern Metal preset NaN issue
bool TestModernMetalIsolation(const fs::path& resourcesDir)
{
  std::cout << "\n========================================================================\n";
  std::cout << "Test: Modern Metal Preset Isolation (NaN Debug)\n";
  std::cout << "========================================================================\n";

  const fs::path dataDir = resourcesDir / "ui" / "data";
  
  // Load preset
  const auto presetsJson = LoadJson(dataDir / "default-presets.json");
  nlohmann::json modernMetalJson;
  for (const auto& preset : presetsJson)
  {
    if (preset.value("name", "") == "Modern Metal")
    {
      modernMetalJson = preset;
      break;
    }
  }
  
  if (modernMetalJson.empty())
  {
    std::cout << "FAIL: Could not find Modern Metal preset\n";
    return false;
  }

  // Parse preset
  auto presetOpt = guitarfx::PresetStorage::DeserializeFromJson(modernMetalJson.dump());
  if (!presetOpt)
  {
    std::cout << "FAIL: Could not parse Modern Metal preset JSON\n";
    return false;
  }
  
  std::cout << "\nPreset Configuration:\n";
  std::cout << "  Input Trim: " << presetOpt->global.inputTrim << " dB\n";
  std::cout << "  Output Trim: " << presetOpt->global.outputTrim << " dB\n";
  std::cout << "  Output Volume: " << presetOpt->global.outputVolume << "\n";
  std::cout << "  Node Count: " << presetOpt->graph.nodes.size() << "\n";
  
  // Print all nodes
  for (const auto& node : presetOpt->graph.nodes)
  {
    std::cout << "  - Node: " << node.id << " (type=" << node.type << ")";
    if (node.resource.has_value())
    {
      const auto& res = node.resource.value();
      std::cout << " -> Resource: type=" << res.resourceType << ", id=" << res.resourceId;
    }
    std::cout << "\n";
  }
  
  // Setup resource library
  const auto audioModelsJson = LoadJson(dataDir / "audiofx-models.json");
  const auto irLibraryJson = LoadJson(dataDir / "ir-library.json");
  
  // Setup DSP
  guitarfx::GraphDSPManager dsp;
  auto& library = dsp.GetResourceLibrary();
  
  // Add NAM model
  for (const auto& entry : audioModelsJson)
  {
    const std::string id = entry.value("id", "");
    if (id == "nam-mesa-recto-modern-g8")
    {
      guitarfx::LibraryResource resource;
      resource.type = "nam";
      resource.id = id;
      resource.name = entry.value("title", id);
      resource.filePath = (resourcesDir / entry.value("filePath", "")).string();
      
      std::cout << "\nNAM Model Resource:\n";
      std::cout << "  ID: " << resource.id << "\n";
      std::cout << "  Path: " << resource.filePath << "\n";
      std::cout << "  Exists: " << (fs::exists(resource.filePath) ? "YES" : "NO") << "\n";
      
      library.AddResource(resource);
      break;
    }
  }
  
  // Add IR
  for (const auto& entry : irLibraryJson)
  {
    const std::string id = entry.value("id", "");
    if (id == "ir-devils-lab-412v30")
    {
      guitarfx::LibraryResource resource;
      resource.type = "ir";
      resource.id = id;
      resource.name = entry.value("title", id);
      resource.filePath = (resourcesDir / entry.value("filePath", "")).string();
      
      std::cout << "\nIR Resource:\n";
      std::cout << "  ID: " << resource.id << "\n";
      std::cout << "  Path: " << resource.filePath << "\n";
      std::cout << "  Exists: " << (fs::exists(resource.filePath) ? "YES" : "NO") << "\n";
      
      library.AddResource(resource);
      break;
    }
  }
  
  dsp.Prepare(kTestSampleRate, kTestBlockSize);
  
  std::cout << "\nLoading preset...\n";
  if (!dsp.LoadPreset(*presetOpt))
  {
    std::cout << "FAIL: LoadPreset returned false\n";
    return false;
  }
  
  std::cout << "Preset loaded successfully\n";
  
  // Check if the preset actually has nodes initialized
  if (!dsp.HasPreset())
  {
    std::cout << "FAIL: HasPreset() returns false after load\n";
    return false;
  }
  
  std::cout << "Preset validated with HasPreset()\n";
  
  // Generate test signal - reuse from generator functions
  std::vector<double> inputL(kTestBlockSize);
  std::vector<double> inputR(kTestBlockSize);
  std::vector<double> outputL(kTestBlockSize, 0.0);
  std::vector<double> outputR(kTestBlockSize, 0.0);

  GenerateSineWave(inputL, 440.0, kTestSampleRate, 0.5);
  GenerateSineWave(inputR, 440.0, kTestSampleRate, 0.5);

  double* inputs[2] = {inputL.data(), inputR.data()};
  double* outputs[2] = {outputL.data(), outputR.data()};

  // Process first block
  std::cout << "\nProcessing block 1...\n";
  
  // Print input signal characteristics
  auto inputAnalysis = AnalyzeSignal(inputL);
  std::cout << "  Input signal: Peak=" << inputAnalysis.peakValue 
            << ", RMS=" << inputAnalysis.rmsValue
            << ", AllZeros=" << (inputAnalysis.isAllZeros ? "Y" : "N") << "\n";
  
  dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), kTestBlockSize);

  auto analysis1 = AnalyzeSignal(outputL);
  std::cout << "  Block 1: Peak=" << std::scientific << analysis1.peakValue << std::defaultfloat
            << ", NaN=" << (analysis1.hasNaN ? "Y" : "N")
            << ", Inf=" << (analysis1.hasInf ? "Y" : "N") << "\n";

  // Check first few samples
  std::cout << "  First 20 samples: ";
  for (int i = 0; i < std::min(20, kTestBlockSize); ++i)
  {
    if (std::isnan(outputL[i]))
      std::cout << "NaN";
    else
      std::cout << outputL[i];
    if (i < 19) std::cout << ", ";
  }
  std::cout << "\n";
  
  // Find where NaN appears
  if (analysis1.hasNaN)
  {
    for (int i = 0; i < kTestBlockSize; ++i)
    {
      if (std::isnan(outputL[i]))
      {
        std::cout << "  First NaN at sample index " << i << "\n";
        break;
      }
    }
  }

  if (analysis1.hasNaN || analysis1.hasInf)
  {
    std::cout << "FAIL: NaN or Inf detected on FIRST block\n";
    
    // Try to understand WHY - check if model loaded
    std::cout << "\nDebug: Checking nodes in graph...\n";
    const auto& graph = presetOpt->graph;
    for (const auto& node : graph.nodes)
    {
      std::cout << "  Node " << node.id << " (type=" << node.type << ")";
      if (node.resource.has_value())
      {
        std::cout << " hasResource=YES";
      }
      else
      {
        std::cout << " hasResource=NO";
      }
      std::cout << "\n";
    }
    
    return false;
  }

  // Process second block
  std::fill(outputL.begin(), outputL.end(), 0.0);
  std::fill(outputR.begin(), outputR.end(), 0.0);
  
  std::cout << "\nProcessing block 2...\n";
  dsp.Process(reinterpret_cast<iplug::sample**>(inputs), reinterpret_cast<iplug::sample**>(outputs), kTestBlockSize);

  auto analysis2 = AnalyzeSignal(outputL);
  std::cout << "  Block 2: Peak=" << std::scientific << analysis2.peakValue << std::defaultfloat
            << ", NaN=" << (analysis2.hasNaN ? "Y" : "N")
            << ", Inf=" << (analysis2.hasInf ? "Y" : "N") << "\n";

  // Check first few samples
  std::cout << "  First 10 samples: ";
  for (int i = 0; i < 10; ++i)
  {
    std::cout << outputL[i];
    if (i < 9) std::cout << ", ";
  }
  std::cout << "\n";

  if (analysis2.hasNaN || analysis2.hasInf)
  {
    std::cout << "FAIL: NaN or Inf detected on SECOND block\n";
    return false;
  }

  std::cout << "\nPASS: Modern Metal isolation test passed\n";
  return true;
}

} // namespace

// ============================================================================
// Main Test Runner
// ============================================================================

int main()
{
#ifndef GUITARFX_TEST_RESOURCES_DIR
#error "GUITARFX_TEST_RESOURCES_DIR must be defined"
#endif
  try
  {
    const fs::path resourcesDir = fs::path(GUITARFX_TEST_RESOURCES_DIR);
    const fs::path dataDir = resourcesDir / "ui" / "data";

    // Run diagnostic tests first
    if (!TestSimpleNAMProcessing(resourcesDir))
    {
      std::cerr << "\nSimple NAM test failed\n";
      return 1;
    }

    if (!TestSimpleNAMWithIR(resourcesDir))
    {
      std::cerr << "\nSimple NAM+IR test failed\n";
      return 1;
    }

    if (!TestProgressiveNodeEnabling(resourcesDir))
    {
      std::cerr << "\nProgressive node test failed\n";
      return 1;
    }

    if (!TestSinglePresetIsolation(resourcesDir))
    {
      std::cerr << "\nSingle preset isolation test failed\n";
      return 1;
    }

    // Run Modern Metal specific test (temporarily disabled to see full test results)
    // if (!TestModernMetalIsolation(resourcesDir))
    // {
    //   std::cerr << "\nModern Metal isolation test failed\n";
    //   return 1;
    // }

    std::vector<std::string> errors;
    const auto recordError = [&errors](std::string message) {
      errors.push_back(std::move(message));
    };

    std::cout << "========================================================================\n";
    std::cout << "Preset DSP Processing Tests - Signal Graph Integration\n";
    std::cout << "========================================================================\n";
    std::cout << "Resources: " << resourcesDir.string() << "\n\n";

    // Load libraries
    const auto audioModelsJson = LoadJson(dataDir / "audiofx-models.json");
    const auto irLibraryJson = LoadJson(dataDir / "ir-library.json");
    const auto presetsJson = LoadJson(dataDir / "default-presets.json");

    if (!presetsJson.is_array())
    {
      throw std::runtime_error("default-presets.json is not an array");
    }

    // Parse all presets upfront
    std::vector<guitarfx::Preset> presets;
    for (const auto& presetJson : presetsJson)
    {
      std::string jsonStr = presetJson.dump();
      auto presetOpt = guitarfx::PresetStorage::DeserializeFromJson(jsonStr);
      if (presetOpt)
      {
        presets.push_back(*presetOpt);
      }
      else
      {
        std::string presetId = presetJson.value("id", "<unknown>");
        recordError("Failed to parse preset: " + presetId);
      }
    }

    std::cout << "Loaded " << presets.size() << " presets from JSON\n\n";

    // Create a single GraphDSPManager to reuse (tests preset switching)
    guitarfx::GraphDSPManager dsp;
    
    // Populate the resource library from JSON files
    auto& library = dsp.GetResourceLibrary();
    
    // Load NAM models into library
    for (const auto& entry : audioModelsJson)
    {
      guitarfx::LibraryResource resource;
      resource.type = "nam";
      resource.id = entry.value("id", "");
      resource.name = entry.value("title", entry.value("name", resource.id));
      resource.category = entry.value("category", "");
      resource.description = entry.value("description", "");
      resource.filePath = resourcesDir / entry.value("filePath", "");
      
      if (!resource.id.empty())
      {
        library.AddResource(resource);
      }
    }
    
    // Load IRs into library
    for (const auto& entry : irLibraryJson)
    {
      guitarfx::LibraryResource resource;
      resource.type = "ir";
      resource.id = entry.value("id", "");
      resource.name = entry.value("title", entry.value("name", resource.id));
      resource.category = entry.value("category", "");
      resource.description = entry.value("description", "");
      resource.filePath = resourcesDir / entry.value("filePath", "");
      
      if (!resource.id.empty())
      {
        library.AddResource(resource);
      }
    }
    
    std::cout << "Resource library populated with " 
              << library.GetResourcesByType("nam").size() << " NAM models and "
              << library.GetResourcesByType("ir").size() << " IRs\n\n";
    
    dsp.Prepare(kTestSampleRate, kTestBlockSize);

    int presetsProcessed = 0;
    int presetsTested = 0;

    std::cout << "Testing DSP processing through signal graph...\n";
    std::cout << "------------------------------------------------------------------------\n";

    for (const auto& preset : presets)
    {
      ++presetsTested;
      std::cout << std::setw(3) << presetsTested << ". " << std::left << std::setw(25) << preset.name;

      // Validate graph structure
      auto graphValidation = ValidatePresetGraph(preset);
      if (!graphValidation.valid)
      {
        std::cout << " SKIP (graph: " << graphValidation.errorMessage << ")\n";
        recordError("Preset '" + preset.name + "': " + graphValidation.errorMessage);
        continue;
      }

      // Validate resources
      auto resourceValidation = ValidatePresetResources(preset, audioModelsJson, irLibraryJson, resourcesDir);
      if (!resourceValidation.valid)
      {
        std::cout << " SKIP (resource: " << resourceValidation.errorMessage << ")\n";
        recordError("Preset '" + preset.name + "': " + resourceValidation.errorMessage);
        continue;
      }

      // Reset DSP state before loading new preset
      dsp.Reset();

      // Load preset into GraphDSPManager
      if (!dsp.LoadPreset(preset))
      {
        std::cout << " FAIL (load failed)\n";
        recordError("Preset '" + preset.name + "': GraphDSPManager::LoadPreset failed");
        continue;
      }

      // Test 1: Basic processing test
      auto basicResult = TestGraphDSPProcessing(dsp, kTestBlockSize, kTestSampleRate);
      if (!basicResult.success)
      {
        std::cout << " FAIL (" << basicResult.errorMessage << ")\n";
        recordError("Preset '" + preset.name + "' processing: " + basicResult.errorMessage);
        continue;
      }

      // Test 2: Stability test (process multiple blocks)
      auto stabilityResult = TestGraphDSPStability(dsp, kTestBlockSize, kTestSampleRate, kStabilityBlocks);
      if (!stabilityResult.success)
      {
        std::cout << " FAIL (" << stabilityResult.errorMessage << ")\n";
        recordError("Preset '" + preset.name + "' stability: " + stabilityResult.errorMessage);
        continue;
      }

      ++presetsProcessed;
      std::cout << " OK (peak=" << std::fixed << std::setprecision(3) 
                << basicResult.outputAnalysis.peakValue 
                << ", rms=" << basicResult.outputAnalysis.rmsValue << ")\n";
    }

    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "DSP Processing Results: " << presetsProcessed << "/" << presetsTested 
              << " presets processed successfully.\n\n";

    // Test preset switching if we have at least 2 presets
    if (presets.size() >= 2)
    {
      std::cout << "Testing preset switching...\n";
      
      auto switchResult = TestPresetSwitching(dsp, presets[0], presets[1], 
                                               kTestBlockSize, kTestSampleRate);
      if (switchResult.success)
      {
        std::cout << "  Preset switching: OK\n";
      }
      else
      {
        std::cout << "  Preset switching: FAIL (" << switchResult.errorMessage << ")\n";
        recordError("Preset switching: " + switchResult.errorMessage);
      }
    }

    std::cout << "\n========================================================================\n";

    if (!errors.empty())
    {
      std::cerr << "\nTest FAILED with " << errors.size() << " issue(s):\n";
      for (const auto& error : errors)
      {
        std::cerr << " - " << error << '\n';
      }
      return 1;
    }

    std::cout << "\nAll preset DSP processing tests PASSED.\n";
    return 0;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "Fatal error: " << ex.what() << std::endl;
    return 1;
  }
}
