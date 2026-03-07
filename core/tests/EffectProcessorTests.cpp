/**
 * @file EffectProcessorTests.cpp
 * @brief Tests for individual effect processors with default settings
 *
 * This test validates that each registered effect processor can:
 * 1. Be created from the registry
 * 2. Be prepared with valid sample rate and block size
 * 3. Process audio without producing NaN, Inf, or silence
 */

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

#include "dsp/EffectRegistry.h"
#include "dsp/EffectProcessor.h"
#include "dsp/EffectGuids.h"
#include "dsp/effects/BuiltinEffects.h"
#include "dsp/effects/AutoArpEffect.h"
#include "dsp/effects/TempoSync.h"

namespace
{

namespace fs = std::filesystem;

constexpr double kTestSampleRate = 48000.0;
constexpr int kTestBlockSize = 512;
constexpr double kPi = 3.14159265358979323846;

// Generate a simple sine wave for testing
void GenerateSineWave(std::vector<float>& buffer, double frequency, double amplitude = 0.5)
{
  for (size_t i = 0; i < buffer.size(); ++i)
  {
    double phase = 2.0 * kPi * frequency * static_cast<double>(i) / kTestSampleRate;
    buffer[i] = static_cast<float>(amplitude * std::sin(phase));
  }
}

// Analyze signal for validity
struct SignalAnalysis
{
  bool hasNaN = false;
  bool hasInf = false;
  bool isAllZeros = false;
  bool isAllSameValue = false;
  double peakValue = 0.0;
  double rmsValue = 0.0;
};

SignalAnalysis AnalyzeSignal(const std::vector<float>& buffer)
{
  SignalAnalysis result;
  
  if (buffer.empty())
    return result;

  double sumSquares = 0.0;
  double peak = 0.0;
  float firstValue = buffer[0];
  bool allSame = true;
  bool allZero = true;

  for (const auto& sample : buffer)
  {
    if (std::isnan(sample))
    {
      result.hasNaN = true;
      return result; // Early exit on NaN
    }
    
    if (std::isinf(sample))
    {
      result.hasInf = true;
      return result; // Early exit on Inf
    }

    double absSample = std::abs(sample);
    if (absSample > peak)
      peak = absSample;
    
    sumSquares += sample * sample;
    
    if (absSample > 1e-10)
      allZero = false;
    
    if (std::abs(sample - firstValue) > 1e-10)
      allSame = false;
  }

  result.peakValue = peak;
  result.rmsValue = std::sqrt(sumSquares / buffer.size());
  result.isAllZeros = allZero;
  result.isAllSameValue = allSame;

  return result;
}

fs::path WriteStereoImpulseToWav(const std::vector<float>& left,
                                 const std::vector<float>& right,
                                 double sampleRate)
{
  const std::size_t length = std::min(left.size(), right.size());
  if (length == 0)
    throw std::runtime_error("Stereo IR must have data");

  const fs::path tempDir = fs::temp_directory_path() / "guitarfx_effect_processor_tests";
  fs::create_directories(tempDir);
  const fs::path path = tempDir / "flanger_reverb_stability_ir.wav";

  std::ofstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("Failed to create temp stereo IR file");

  std::vector<float> interleaved(length * 2);
  for (std::size_t i = 0; i < length; ++i)
  {
    interleaved[i * 2] = left[i];
    interleaved[i * 2 + 1] = right[i];
  }

  const uint32_t dataSize = static_cast<uint32_t>(interleaved.size() * sizeof(float));
  const uint32_t riffSize = 36u + dataSize;
  const uint16_t audioFormat = 3;
  const uint16_t numChannels = 2;
  const uint32_t sampleRateU32 = static_cast<uint32_t>(sampleRate);
  const uint32_t byteRate = sampleRateU32 * numChannels * sizeof(float);
  const uint16_t blockAlign = static_cast<uint16_t>(numChannels * sizeof(float));
  const uint16_t bitsPerSample = 32;
  const uint32_t fmtChunkSize = 16;

  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&riffSize), 4);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  file.write(reinterpret_cast<const char*>(&fmtChunkSize), 4);
  file.write(reinterpret_cast<const char*>(&audioFormat), 2);
  file.write(reinterpret_cast<const char*>(&numChannels), 2);
  file.write(reinterpret_cast<const char*>(&sampleRateU32), 4);
  file.write(reinterpret_cast<const char*>(&byteRate), 4);
  file.write(reinterpret_cast<const char*>(&blockAlign), 2);
  file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&dataSize), 4);
  file.write(reinterpret_cast<const char*>(interleaved.data()), dataSize);

  return path;
}

bool TestEffectProcessor(const std::string& effectType)
{
  auto& registry = guitarfx::EffectRegistry::Instance();
  
  // Create effect
  auto effect = registry.Create(effectType);
  if (!effect)
  {
    std::cout << "  ERROR: Failed to create effect\n";
    return false;
  }

  // Prepare effect
  effect->Prepare(kTestSampleRate, kTestBlockSize);
  effect->Reset();

  // Create test buffers
  std::vector<float> outputL(kTestBlockSize, 0.0f);
  std::vector<float> outputR(kTestBlockSize, 0.0f);

  const int latencyBlocks = std::max(0, (effect->GetLatencySamples() + kTestBlockSize - 1) / kTestBlockSize);
  const int blocksToProcess = std::max((effectType == guitarfx::EffectGuids::kSynthSaw) ? 8 : 1,
                                       latencyBlocks + 2);

  // Generate a phase-continuous 440 Hz sine across all blocks so that
  // pitch-tracking effects (YIN) do not see a phase discontinuity at each
  // block boundary which would corrupt the autocorrelation.
  const int totalSamples = kTestBlockSize * blocksToProcess;
  std::vector<float> inputL(static_cast<size_t>(totalSamples));
  std::vector<float> inputR(static_cast<size_t>(totalSamples));
  GenerateSineWave(inputL, 440.0, 0.5);
  GenerateSineWave(inputR, 440.0, 0.5);

  float* outputs[2] = {outputL.data(), outputR.data()};

  try
  {
    for (int block = 0; block < blocksToProcess; ++block)
    {
      std::fill(outputL.begin(), outputL.end(), 0.0f);
      std::fill(outputR.begin(), outputR.end(), 0.0f);
      float* blkInputs[2] = {
        inputL.data() + block * kTestBlockSize,
        inputR.data() + block * kTestBlockSize
      };
      effect->Process(blkInputs, outputs, kTestBlockSize);
    }
  }
  catch (const std::exception& ex)
  {
    std::cout << "  ERROR: Process threw exception: " << ex.what() << "\n";
    return false;
  }

  // Analyze output
  auto inputAnalysis = AnalyzeSignal(inputL);
  auto outputAnalysis = AnalyzeSignal(outputL);

  if (outputAnalysis.hasNaN)
  {
    std::cout << "  FAIL: Output contains NaN\n";
    return false;
  }

  if (outputAnalysis.hasInf)
  {
    std::cout << "  FAIL: Output contains Inf\n";
    return false;
  }

  if (outputAnalysis.isAllZeros)
  {
    std::cout << "  FAIL: Output is all zeros (no signal)\n";
    return false;
  }

  if (outputAnalysis.peakValue > 100.0)
  {
    std::cout << "  FAIL: Output peak excessively high (" << outputAnalysis.peakValue << ")\n";
    return false;
  }

  // Success - output is valid
  std::cout << "  PASS (peak=" << std::fixed << std::setprecision(3) 
            << outputAnalysis.peakValue << ", rms=" << outputAnalysis.rmsValue << ")\n";
  return true;
}

} // anonymous namespace

// ════════════════════════════════════════════════════════════════════
// Auto-Arpeggiator specific tests
// ════════════════════════════════════════════════════════════════════

namespace
{

bool TestAutoArpSpecific()
{
  std::cout << "\n--- AutoArpEffect Specific Tests ---\n";
  int passed = 0, failed = 0;

  auto makeArp = []() -> std::unique_ptr<guitarfx::AutoArpEffect> {
    auto e = std::make_unique<guitarfx::AutoArpEffect>();
    e->Prepare(kTestSampleRate, kTestBlockSize);
    return e;
  };

  std::vector<float> inL(kTestBlockSize), inR(kTestBlockSize);
  std::vector<float> outL(kTestBlockSize, 0.f), outR(kTestBlockSize, 0.f);
  GenerateSineWave(inL, 440.0, 0.5);
  GenerateSineWave(inR, 440.0, 0.5);
  float* ins[2]  = {inL.data(), inR.data()};
  float* outs[2] = {outL.data(), outR.data()};

  // Test 1: SetParam round-trip for timing params
  {
    auto e = makeArp();
    e->SetParam("bpm",      130.0);
    e->SetParam("stepRate", 2.0);
    e->SetParam("numSteps", 3.0);
    const bool ok = (e->GetParam("bpm") == 130.0) &&
                    (e->GetParam("stepRate") == 2.0) &&
                    (e->GetParam("numSteps") == 3.0);
    std::cout << "  SetParam round-trip (bpm/stepRate/numSteps): " << (ok ? "PASS" : "FAIL") << "\n";
    ok ? ++passed : ++failed;
  }

  // Test 2: Process produces non-zero output (step 0 = 0 st, bypass path)
  {
    auto e = makeArp();
    std::fill(outL.begin(), outL.end(), 0.f);
    std::fill(outR.begin(), outR.end(), 0.f);
    e->Process(ins, outs, kTestBlockSize);
    const auto analysis = AnalyzeSignal(outL);
    const bool ok = !analysis.isAllZeros && !analysis.hasNaN && !analysis.hasInf;
    std::cout << "  Process non-zero output (bypass path):       " << (ok ? "PASS" : "FAIL") << "\n";
    ok ? ++passed : ++failed;
  }

  // Test 3: Reset clears phase — two separate instances started at same time
  //         should produce identical output right after Reset().
  {
    auto e = makeArp();
    e->SetParam("bpm", 90.0);
    // Process a few blocks to advance phase
    for (int b = 0; b < 4; ++b)
      e->Process(ins, outs, kTestBlockSize);
    e->Reset();
    // After reset, replaying the same input should match a freshly-prepared instance
    auto fresh = makeArp();
    fresh->SetParam("bpm", 90.0);

    std::vector<float> out1(kTestBlockSize, 0.f), out1r(kTestBlockSize, 0.f);
    std::vector<float> out2(kTestBlockSize, 0.f), out2r(kTestBlockSize, 0.f);
    float* o1[2] = {out1.data(), out1r.data()};
    float* o2[2] = {out2.data(), out2r.data()};
    e->Process(ins, o1, kTestBlockSize);
    fresh->Process(ins, o2, kTestBlockSize);

    bool match = true;
    for (int i = 0; i < kTestBlockSize && match; ++i)
      if (std::abs(out1[static_cast<size_t>(i)] - out2[static_cast<size_t>(i)]) > 1e-5f)
        match = false;
    std::cout << "  Reset restores initial state:                " << (match ? "PASS" : "FAIL") << "\n";
    match ? ++passed : ++failed;
  }

  // Test 4: Custom pattern — changing step0 changes behaviour
  {
    auto e = makeArp();
    e->SetParam("pattern", 4.0);   // Custom
    e->SetParam("step0",   0.0);   // Root = bypass stretch
    e->SetParam("numSteps", 2.0);
    e->SetParam("step1",   0.0);   // All bypass
    e->SetParam("gate",    1.0);   // Gate fully open
    e->SetParam("attack",  0.0);
    e->SetParam("mix",     1.0);
    std::fill(outL.begin(), outL.end(), 0.f);
    std::fill(outR.begin(), outR.end(), 0.f);
    e->Process(ins, outs, kTestBlockSize);
    const auto analysis = AnalyzeSignal(outL);
    const bool ok = !analysis.isAllZeros;
    std::cout << "  Custom pattern step0=0 produces output:      " << (ok ? "PASS" : "FAIL") << "\n";
    ok ? ++passed : ++failed;
  }

  // Test 5: Switching back to predefined pattern
  {
    auto e = makeArp();
    e->SetParam("pattern", 4.0);  // Custom
    e->SetParam("pattern", 0.0);  // Back to Major Triad — should not crash
    std::fill(outL.begin(), outL.end(), 0.f);
    std::fill(outR.begin(), outR.end(), 0.f);
    e->Process(ins, outs, kTestBlockSize);
    const auto analysis = AnalyzeSignal(outL);
    const bool ok = !analysis.hasNaN && !analysis.hasInf;
    std::cout << "  Switch back to predefined pattern (no NaN):  " << (ok ? "PASS" : "FAIL") << "\n";
    ok ? ++passed : ++failed;
  }

  // Test 6: BPM injection via SetParam("bpm") at multiple BPMs
  {
    bool allOk = true;
    for (const double bpm : {60.0, 120.0, 180.0, 300.0})
    {
      auto e = makeArp();
      e->SetParam("bpm", bpm);
      std::fill(outL.begin(), outL.end(), 0.f);
      std::fill(outR.begin(), outR.end(), 0.f);
      e->Process(ins, outs, kTestBlockSize);
      const auto analysis = AnalyzeSignal(outL);
      if (analysis.hasNaN || analysis.hasInf)
        allOk = false;
    }
    std::cout << "  BPM range 60-300 no NaN/Inf:                 " << (allOk ? "PASS" : "FAIL") << "\n";
    allOk ? ++passed : ++failed;
  }

  std::cout << "AutoArp specific: " << passed << "/" << (passed + failed) << " passed.\n";
  return failed == 0;
}

bool TestFlangerReverbStability()
{
  std::cout << "\n--- Flanger -> IR Reverb Stability Test ---\n";

  auto& registry = guitarfx::EffectRegistry::Instance();
  auto flanger = registry.Create(guitarfx::EffectGuids::kFlanger);
  auto reverb = registry.Create(guitarfx::EffectGuids::kReverbIr);
  if (!flanger || !reverb)
  {
    std::cout << "  FAIL: Could not create flanger or IR reverb\n";
    return false;
  }

  constexpr std::size_t kIRLength = 4096;
  std::vector<float> irL(kIRLength, 0.0f);
  std::vector<float> irR(kIRLength, 0.0f);
  for (std::size_t i = 0; i < kIRLength; ++i)
  {
    const float t = static_cast<float>(i) / static_cast<float>(kIRLength);
    const float env = std::exp(-6.0f * t);
    const float modA = std::sin(2.0 * kPi * 0.013 * static_cast<double>(i));
    const float modB = std::cos(2.0 * kPi * 0.021 * static_cast<double>(i));
    irL[i] = env * static_cast<float>(0.65 * modA + 0.35 * modB);
    irR[i] = env * static_cast<float>(0.60 * modB - 0.30 * modA);
  }
  irL[0] += 0.6f;
  irR[0] += 0.6f;

  const fs::path irPath = WriteStereoImpulseToWav(irL, irR, kTestSampleRate);

  try
  {
    flanger->Prepare(kTestSampleRate, kTestBlockSize);
    reverb->Prepare(kTestSampleRate, kTestBlockSize);

    flanger->SetParam("rate", 0.25);
    flanger->SetParam("depth", 5.0);
    flanger->SetParam("delay", 5.0);
    flanger->SetParam("feedback", 0.85);
    flanger->SetParam("mix", 1.0);

    reverb->SetParam("mix", 1.0);
    reverb->SetParam("outputGain", 0.0);
    if (!reverb->LoadResource(irPath))
    {
      std::cout << "  FAIL: Could not load IR resource\n";
      fs::remove(irPath);
      return false;
    }

    std::vector<float> inputL(kTestBlockSize, 0.0f);
    std::vector<float> inputR(kTestBlockSize, 0.0f);
    std::vector<float> flangerL(kTestBlockSize, 0.0f);
    std::vector<float> flangerR(kTestBlockSize, 0.0f);
    std::vector<float> reverbL(kTestBlockSize, 0.0f);
    std::vector<float> reverbR(kTestBlockSize, 0.0f);

    float* inPtrs[2] = {inputL.data(), inputR.data()};
    float* flangerOutPtrs[2] = {flangerL.data(), flangerR.data()};
    float* reverbOutPtrs[2] = {reverbL.data(), reverbR.data()};

    constexpr int kDurationSeconds = 8;
    const int totalBlocks = static_cast<int>((kDurationSeconds * kTestSampleRate) / kTestBlockSize);
    double peak = 0.0;
    double sumSquares = 0.0;
    std::size_t sampleCount = 0;

    for (int block = 0; block < totalBlocks; ++block)
    {
      const std::size_t startIndex = static_cast<std::size_t>(block * kTestBlockSize);
      for (int i = 0; i < kTestBlockSize; ++i)
      {
        const double phase = 2.0 * kPi * 110.0 * static_cast<double>(startIndex + static_cast<std::size_t>(i)) / kTestSampleRate;
        const float sample = static_cast<float>(0.8 * std::sin(phase));
        inputL[static_cast<std::size_t>(i)] = sample;
        inputR[static_cast<std::size_t>(i)] = sample;
      }

      flanger->Process(inPtrs, flangerOutPtrs, kTestBlockSize);

      float* reverbInPtrs[2] = {flangerL.data(), flangerR.data()};
      reverb->Process(reverbInPtrs, reverbOutPtrs, kTestBlockSize);

      for (int i = 0; i < kTestBlockSize; ++i)
      {
        const float samples[2] = {reverbL[static_cast<std::size_t>(i)], reverbR[static_cast<std::size_t>(i)]};
        for (float sample : samples)
        {
          if (!std::isfinite(sample))
          {
            std::cout << "  FAIL: Non-finite sample at block " << block << "\n";
            fs::remove(irPath);
            return false;
          }

          peak = std::max(peak, static_cast<double>(std::abs(sample)));
          sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
          ++sampleCount;
        }
      }
    }

    fs::remove(irPath);

    const double rms = sampleCount > 0 ? std::sqrt(sumSquares / static_cast<double>(sampleCount)) : 0.0;
    const bool ok = peak < 12.0 && rms < 4.0;
    std::cout << "  Peak=" << std::fixed << std::setprecision(3) << peak
              << ", RMS=" << rms << " -> " << (ok ? "PASS" : "FAIL") << "\n";
    return ok;
  }
  catch (const std::exception& ex)
  {
    fs::remove(irPath);
    std::cout << "  FAIL: Exception during flanger/reverb stability test: " << ex.what() << "\n";
    return false;
  }
}

bool TestTempoSyncSpecific()
{
  std::cout << "\n--- Tempo Sync Effect Tests ---\n";

  struct TempoCase
  {
    const char* label;
    const std::string type;
    double bpm;
    double division;
    const char* effectiveKey;
    double expected;
    double tolerance;
  };

  const std::vector<TempoCase> cases = {
    {"Delay quarter-note at 120 BPM", guitarfx::EffectGuids::kDelayDigital, 120.0, 4.0, "effectiveTimeMs", 500.0, 0.01},
    {"Tremolo quarter-note at 120 BPM", guitarfx::EffectGuids::kTremolo, 120.0, 4.0, "effectiveRate", 2.0, 0.01},
    {"Chorus half-note at 120 BPM", guitarfx::EffectGuids::kChorus, 120.0, 1.0, "effectiveRate", 1.0, 0.01},
    {"Flanger whole-note at 120 BPM", guitarfx::EffectGuids::kFlanger, 120.0, 0.0, "effectiveRate", 0.5, 0.01},
    {"Phaser eighth-note triplet at 120 BPM", guitarfx::EffectGuids::kPhaser, 120.0, 9.0, "effectiveRate", 6.0, 0.01},
  };

  auto& registry = guitarfx::EffectRegistry::Instance();
  int passed = 0;
  int failed = 0;

  std::vector<float> inputL(kTestBlockSize, 0.0f);
  std::vector<float> inputR(kTestBlockSize, 0.0f);
  std::vector<float> outputL(kTestBlockSize, 0.0f);
  std::vector<float> outputR(kTestBlockSize, 0.0f);
  GenerateSineWave(inputL, 220.0, 0.5);
  GenerateSineWave(inputR, 220.0, 0.5);
  float* inputs[2] = {inputL.data(), inputR.data()};
  float* outputs[2] = {outputL.data(), outputR.data()};

  for (const auto& testCase : cases)
  {
    auto effect = registry.Create(testCase.type);
    if (!effect)
    {
      std::cout << "  " << std::left << std::setw(44) << testCase.label << "FAIL (create)\n";
      ++failed;
      continue;
    }

    effect->Prepare(kTestSampleRate, kTestBlockSize);
    effect->SetParam("syncMode", 1.0);
    effect->SetParam("syncDivision", testCase.division);
    effect->SetParam("bpm", testCase.bpm);

    const double effective = effect->GetParam(testCase.effectiveKey);
    const bool mappingOk = std::abs(effective - testCase.expected) <= testCase.tolerance;

    std::fill(outputL.begin(), outputL.end(), 0.0f);
    std::fill(outputR.begin(), outputR.end(), 0.0f);
    effect->Process(inputs, outputs, kTestBlockSize);
    const auto analysis = AnalyzeSignal(outputL);
    const bool audioOk = !analysis.hasNaN && !analysis.hasInf;

    const bool ok = mappingOk && audioOk;
    std::cout << "  " << std::left << std::setw(44) << testCase.label
              << (ok ? "PASS" : "FAIL")
              << " (effective=" << std::fixed << std::setprecision(3) << effective << ")\n";
    ok ? ++passed : ++failed;
  }

  std::cout << "Tempo-sync specific: " << passed << "/" << (passed + failed) << " passed.\n";
  return failed == 0;
}

bool TestOctaveSpecific()
{
  std::cout << "\n--- OctaveEffect Specific Tests ---\n";

  auto& registry = guitarfx::EffectRegistry::Instance();

  auto runVoiceTest = [&](double octaveUp, double octaveDown, const char* label)
  {
    auto effect = registry.Create(guitarfx::EffectGuids::kOctave);
    if (!effect)
    {
      std::cout << "  FAIL: Could not create octave effect\n";
      return false;
    }

    effect->Prepare(kTestSampleRate, kTestBlockSize);
    effect->SetParam("octaveUp", octaveUp);
    effect->SetParam("octaveDown", octaveDown);
    effect->SetParam("tone", 1.0);
    effect->SetParam("mix", 1.0);

    if (effect->GetLatencySamples() <= 0)
    {
      std::cout << "  FAIL: " << label << " reports no pitch-shift latency\n";
      return false;
    }

    constexpr int kBlocksToProcess = 8;
    const int totalSamples = kTestBlockSize * kBlocksToProcess;
    std::vector<float> inputL(static_cast<size_t>(totalSamples));
    std::vector<float> inputR(static_cast<size_t>(totalSamples));
    GenerateSineWave(inputL, 220.0, 0.5);
    GenerateSineWave(inputR, 220.0, 0.5);

    std::vector<float> outputL(kTestBlockSize, 0.0f);
    std::vector<float> outputR(kTestBlockSize, 0.0f);
    float* outputs[2] = {outputL.data(), outputR.data()};

    for (int block = 0; block < kBlocksToProcess; ++block)
    {
      float* inputs[2] = {
        inputL.data() + block * kTestBlockSize,
        inputR.data() + block * kTestBlockSize
      };
      effect->Process(inputs, outputs, kTestBlockSize);
    }

    const auto analysis = AnalyzeSignal(outputL);
    const bool ok = !analysis.hasNaN && !analysis.hasInf && !analysis.isAllZeros && analysis.peakValue > 0.01;
    std::cout << "  " << std::left << std::setw(44) << label << (ok ? "PASS" : "FAIL")
              << " (latency=" << effect->GetLatencySamples()
              << ", peak=" << std::fixed << std::setprecision(3) << analysis.peakValue << ")\n";
    return ok;
  };

  const bool octaveUpOk = runVoiceTest(1.0, 0.0, "Octave-up voice produces shifted output:");
  const bool octaveDownOk = runVoiceTest(0.0, 1.0, "Octave-down voice produces shifted output:");
  return octaveUpOk && octaveDownOk;
}

} // anonymous namespace

int main()
{
  std::cout << "========================================\n";
  std::cout << "Effect Processor Tests\n";
  std::cout << "========================================\n\n";

  // Register all effects before testing
  guitarfx::RegisterAllEffects();

  auto& registry = guitarfx::EffectRegistry::Instance();
  auto allTypes = registry.GetAllTypes();

  if (allTypes.empty())
  {
    std::cerr << "ERROR: No effects registered!\n";
    return 1;
  }

  std::cout << "Testing " << allTypes.size() << " registered effects...\n\n";

  int passed = 0;
  int failed = 0;
  std::vector<std::string> failedEffects;

  for (const auto& info : allTypes)
  {
    std::cout << std::left << std::setw(30) << info.displayName << " (" << info.type << ")";
    
    if (TestEffectProcessor(info.type))
    {
      ++passed;
    }
    else
    {
      ++failed;
      failedEffects.push_back(info.type);
    }
  }

  std::cout << "\n========================================\n";
  std::cout << "Results: " << passed << "/" << allTypes.size() << " effects passed\n";
  
  if (failed > 0)
  {
    std::cout << "\nFailed effects:\n";
    for (const auto& name : failedEffects)
    {
      std::cout << "  - " << name << "\n";
    }
    std::cout << "\n";
    return 1;
  }

  std::cout << "\nAll effects PASSED.\n";

  // AutoArpeggiator-specific behavioural tests
  if (!TestAutoArpSpecific())
    return 1;

  if (!TestOctaveSpecific())
    return 1;

  if (!TestTempoSyncSpecific())
    return 1;

  if (!TestFlangerReverbStability())
    return 1;

  return 0;
}
