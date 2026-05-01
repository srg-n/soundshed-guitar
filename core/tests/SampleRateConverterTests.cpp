#include "dsp/BlockSincResampler.h"
#include "dsp/effects/NAMSampleRate.h"
#include "dsp/simd/OptimizedNAM.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
  constexpr double kPi = 3.14159265358979323846;

  void GenerateSine(std::vector<float>& buffer, double frequency, double sampleRate)
  {
    for (std::size_t sampleIndex = 0; sampleIndex < buffer.size(); ++sampleIndex)
    {
      buffer[sampleIndex] = static_cast<float>(0.5 * std::sin(2.0 * kPi * frequency * static_cast<double>(sampleIndex) / sampleRate));
    }
  }

  bool BufferIsFinite(const std::vector<float>& buffer)
  {
    for (const float sample : buffer)
    {
      if (!std::isfinite(sample))
        return false;
    }
    return true;
  }

  double RmsError(const std::vector<float>& first, const std::vector<float>& second)
  {
    const std::size_t count = std::min(first.size(), second.size());
    if (count == 0)
      return std::numeric_limits<double>::infinity();

    double sumSquares = 0.0;
    for (std::size_t sampleIndex = 0; sampleIndex < count; ++sampleIndex)
    {
      const double delta = static_cast<double>(first[sampleIndex]) - static_cast<double>(second[sampleIndex]);
      sumSquares += delta * delta;
    }
    return std::sqrt(sumSquares / static_cast<double>(count));
  }

  bool TestRoundTripQuality()
  {
    constexpr double sourceRate = 48000.0;
    constexpr double intermediateRate = 44100.0;
    constexpr int sourceFrames = 2048;
    constexpr int intermediateFrames = 1882;

    std::vector<float> source(sourceFrames);
    std::vector<float> intermediate(intermediateFrames);
    std::vector<float> roundTrip(sourceFrames);

    GenerateSine(source, 997.0, sourceRate);

    guitarfx::BlockSincResampler downsampler;
    downsampler.Prepare(sourceRate, intermediateRate, sourceFrames);
    downsampler.ProcessFixedOutput(source.data(), static_cast<int>(source.size()), intermediate.data(), static_cast<int>(intermediate.size()));

    guitarfx::BlockSincResampler upsampler;
    upsampler.Prepare(intermediateRate, sourceRate, intermediateFrames);
    upsampler.ProcessFixedOutput(intermediate.data(), static_cast<int>(intermediate.size()), roundTrip.data(), static_cast<int>(roundTrip.size()));

    const double error = RmsError(source, roundTrip);
    std::cout << "Round-trip RMS error: " << error << "\n";
    return BufferIsFinite(intermediate) && BufferIsFinite(roundTrip) && error < 0.02;
  }

  bool TestFixedOutputCount()
  {
    constexpr double sourceRate = 44100.0;
    constexpr double targetRate = 96000.0;
    constexpr int sourceFrames = 257;
    constexpr int targetFrames = 559;

    std::vector<float> source(sourceFrames);
    std::vector<float> target(targetFrames, 0.0f);
    GenerateSine(source, 440.0, sourceRate);

    guitarfx::BlockSincResampler resampler;
    resampler.Prepare(sourceRate, targetRate, sourceFrames);
    const int written = resampler.ProcessFixedOutput(source.data(), sourceFrames, target.data(), targetFrames);

    return written == targetFrames && BufferIsFinite(target);
  }

  bool TestOptimizedNamSampleRateParsing()
  {
    const nlohmann::json metadataRate = nlohmann::json::parse(
      R"({"metadata":{"expected_sample_rate":"96000"},"sample_rate":48000,"config":{"sample_rate":44100}})");
    const nlohmann::json topLevelRate = nlohmann::json::parse(
      R"({"sample_rate":48000,"config":{}})");
    const nlohmann::json configRate = nlohmann::json::parse(
      R"({"config":{"sample_rate":44100}})");
    const nlohmann::json missingRate = nlohmann::json::parse(
      R"({"config":{}})");

    return guitarfx::nam::ReadExpectedSampleRateFromNamJson(metadataRate) == 96000.0
      && guitarfx::nam::ReadExpectedSampleRateFromNamJson(topLevelRate) == 48000.0
      && guitarfx::nam::ReadExpectedSampleRateFromNamJson(configRate) == 44100.0
      && guitarfx::nam::ReadExpectedSampleRateFromNamJson(missingRate) < 0.0;
  }

  bool TestNamDefaultProcessingRate()
  {
    return guitarfx::ResolveNamModelProcessingSampleRate(44100.0, 96000.0) == 44100.0
      && guitarfx::ResolveNamModelProcessingSampleRate(-1.0, 96000.0) == guitarfx::kDefaultNamModelSampleRate;
  }
}

int main()
{
  const bool roundTripOk = TestRoundTripQuality();
  const bool fixedOutputOk = TestFixedOutputCount();
  const bool optimizedNamSampleRateParsingOk = TestOptimizedNamSampleRateParsing();
  const bool namDefaultProcessingRateOk = TestNamDefaultProcessingRate();

  if (!roundTripOk)
    std::cerr << "Sample-rate converter round-trip quality test failed\n";
  if (!fixedOutputOk)
    std::cerr << "Sample-rate converter fixed output count test failed\n";
  if (!optimizedNamSampleRateParsingOk)
    std::cerr << "Optimized NAM sample-rate metadata parsing test failed\n";
  if (!namDefaultProcessingRateOk)
    std::cerr << "NAM default processing-rate test failed\n";

  return (roundTripOk && fixedOutputOk && optimizedNamSampleRateParsingOk && namDefaultProcessingRateOk) ? 0 : 1;
}