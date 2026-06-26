#pragma once

#include "dsp/EffectGuids.h"
#include "dsp/EffectProcessor.h"
#include "dsp/EffectRegistry.h"
#include "dsp/LevelTargets.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace guitarfx
{
  /**
   * Utility pass-through node that also computes live analyzer telemetry.
   * The signal is forwarded unchanged while this processor captures level
   * conversions and a rolling spectrogram snapshot for UI rendering.
   */
  class InputAnalyzerEffect : public EffectProcessor
  {
  public:
    static constexpr int kSpectrogramBins = 64;
    static constexpr double kSpectrogramMinDbfs = -120.0;
    static constexpr double kSpectrogramMaxDbfs = 0.0;
    static constexpr double kSpectrogramMinFrequencyHz = 20.0;
    static constexpr double kSpectrogramMaxFrequencyHz = 20000.0;

    struct TelemetrySnapshot
    {
      bool valid = false;
      double peakPercent = 0.0;
      double rmsPercent = 0.0;
      double rmsDbu = 0.0;
      double rmsDbv = 0.0;
      double rmsVolts = 0.0;
      std::array<float, kSpectrogramBins> spectrogramBinsDb{};
      std::uint64_t generatedAtMs = 0;
    };

    void Prepare(double sampleRate, int maxBlockSize) override
    {
      if (!ValidatePrepare(sampleRate, maxBlockSize))
      {
        return;
      }
      mSampleRate = sampleRate;
      mMaxBlockSize = maxBlockSize;
      Reset();
    }

    void Reset() override
    {
      mTelemetryValid.store(false, std::memory_order_relaxed);
      mPeakPercent.store(0.0, std::memory_order_relaxed);
      mRmsPercent.store(0.0, std::memory_order_relaxed);
      mRmsDbu.store(0.0, std::memory_order_relaxed);
      mRmsDbv.store(0.0, std::memory_order_relaxed);
      mRmsVolts.store(0.0, std::memory_order_relaxed);
      mGeneratedAtMs.store(0, std::memory_order_relaxed);
      mSmoothedPeakLinear = 0.0;
      mSmoothedRmsLinear = 0.0;
      for (auto &bin : mSpectrogramBinsDb)
      {
        bin.store(static_cast<float>(kSpectrogramMinDbfs), std::memory_order_relaxed);
      }
    }

    void Process(float **inputs, float **outputs, int numSamples) override
    {
      if (!outputs)
      {
        return;
      }

      const float *leftIn = (inputs && inputs[0]) ? inputs[0] : nullptr;
      const float *rightIn = (inputs && inputs[1]) ? inputs[1] : nullptr;
      const bool hasSignalInput = leftIn != nullptr || rightIn != nullptr;

      for (int ch = 0; ch < 2; ++ch)
      {
        float *out = outputs[ch];
        const float *in = inputs ? inputs[ch] : nullptr;
        if (!out)
        {
          continue;
        }

        if (in)
        {
          for (int i = 0; i < numSamples; ++i)
          {
            out[i] = in[i];
          }
        }
        else
        {
          for (int i = 0; i < numSamples; ++i)
          {
            out[i] = 0.0f;
          }
        }
      }

      if (!hasSignalInput || numSamples <= 0)
      {
        mTelemetryValid.store(false, std::memory_order_relaxed);
        return;
      }

      double peak = 0.0;
      double sumSquares = 0.0;
      std::size_t sampleCount = 0;

      if (leftIn)
      {
        for (int i = 0; i < numSamples; ++i)
        {
          const double value = static_cast<double>(leftIn[i]);
          peak = std::max(peak, std::abs(value));
          sumSquares += value * value;
        }
        sampleCount += static_cast<std::size_t>(numSamples);
      }

      if (rightIn)
      {
        for (int i = 0; i < numSamples; ++i)
        {
          const double value = static_cast<double>(rightIn[i]);
          peak = std::max(peak, std::abs(value));
          sumSquares += value * value;
        }
        sampleCount += static_cast<std::size_t>(numSamples);
      }

      const double rms = sampleCount > 0
        ? std::sqrt(sumSquares / static_cast<double>(sampleCount))
        : 0.0;

      const double deltaSeconds = (mSampleRate > 0.0)
        ? static_cast<double>(numSamples) / mSampleRate
        : 0.0;
      mSmoothedPeakLinear = SmoothWithBallistics(
        std::max(0.0, peak),
        mSmoothedPeakLinear,
        deltaSeconds,
        kPeakAttackSeconds,
        kPeakReleaseSeconds);
      mSmoothedRmsLinear = SmoothWithBallistics(
        std::max(0.0, rms),
        mSmoothedRmsLinear,
        deltaSeconds,
        kRmsAttackSeconds,
        kRmsReleaseSeconds);

      const double nominalDbfs = GetNominalOperatingLevelDbfs();
      const double rmsDbfs = ToDbfs(mSmoothedRmsLinear);
      const double rmsDbu = rmsDbfs - nominalDbfs + 4.0;
      const double rmsDbv = rmsDbu - kDbuToDbvOffset;
      const double rmsVolts = 0.775 * std::pow(10.0, rmsDbu / 20.0);

      mPeakPercent.store(std::clamp(mSmoothedPeakLinear * 100.0, 0.0, 100.0), std::memory_order_relaxed);
      mRmsPercent.store(std::clamp(mSmoothedRmsLinear * 100.0, 0.0, 100.0), std::memory_order_relaxed);
      mRmsDbu.store(rmsDbu, std::memory_order_relaxed);
      mRmsDbv.store(rmsDbv, std::memory_order_relaxed);
      mRmsVolts.store(std::max(0.0, rmsVolts), std::memory_order_relaxed);

      const int sampleWindow = std::max(1, numSamples);
      const double logRange = std::log(kSpectrogramMaxFrequencyHz / kSpectrogramMinFrequencyHz);
      constexpr float kSmoothing = 0.72f;
      constexpr float kOneMinusSmoothing = 1.0f - kSmoothing;

      for (int bin = 0; bin < kSpectrogramBins; ++bin)
      {
        const double t = (kSpectrogramBins <= 1)
          ? 0.0
          : static_cast<double>(bin) / static_cast<double>(kSpectrogramBins - 1);
        const double freq = kSpectrogramMinFrequencyHz * std::exp(logRange * t);
        const double omega = (2.0 * kPi * freq) / std::max(1.0, mSampleRate);
        const double coeff = 2.0 * std::cos(omega);
        double s0 = 0.0;
        double s1 = 0.0;
        double s2 = 0.0;

        for (int i = 0; i < sampleWindow; ++i)
        {
          const double left = leftIn ? static_cast<double>(leftIn[i]) : 0.0;
          const double right = rightIn ? static_cast<double>(rightIn[i]) : left;
          const double mono = 0.5 * (left + right);
          const double window = 0.5 - 0.5 * std::cos((2.0 * kPi * static_cast<double>(i)) / static_cast<double>(sampleWindow));
          s0 = (mono * window) + coeff * s1 - s2;
          s2 = s1;
          s1 = s0;
        }

        const double power = std::max(0.0, (s1 * s1) + (s2 * s2) - (coeff * s1 * s2));
        const double magnitude = std::sqrt(power) / static_cast<double>(sampleWindow);
        const float magnitudeDb = static_cast<float>(std::clamp(ToDbfs(magnitude), kSpectrogramMinDbfs, kSpectrogramMaxDbfs));
        const float previous = mSpectrogramBinsDb[static_cast<std::size_t>(bin)].load(std::memory_order_relaxed);
        const float smoothed = previous * kSmoothing + magnitudeDb * kOneMinusSmoothing;
        mSpectrogramBinsDb[static_cast<std::size_t>(bin)].store(smoothed, std::memory_order_relaxed);
      }

      mGeneratedAtMs.store(
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count()),
        std::memory_order_relaxed);
      mTelemetryValid.store(true, std::memory_order_relaxed);
    }

    void SetParam(const std::string &, double) override {}
    void SetConfig(const std::string &, const std::string &) override {}
    [[nodiscard]] double GetParam(const std::string &) const override { return 0.0; }
    [[nodiscard]] std::string GetType() const override { return "input_analyzer"; }
    [[nodiscard]] std::string GetCategory() const override { return "utility"; }

    [[nodiscard]] TelemetrySnapshot GetTelemetrySnapshot() const
    {
      TelemetrySnapshot snapshot;
      snapshot.valid = mTelemetryValid.load(std::memory_order_relaxed);
      snapshot.peakPercent = mPeakPercent.load(std::memory_order_relaxed);
      snapshot.rmsPercent = mRmsPercent.load(std::memory_order_relaxed);
      snapshot.rmsDbu = mRmsDbu.load(std::memory_order_relaxed);
      snapshot.rmsDbv = mRmsDbv.load(std::memory_order_relaxed);
      snapshot.rmsVolts = mRmsVolts.load(std::memory_order_relaxed);
      for (int i = 0; i < kSpectrogramBins; ++i)
      {
        snapshot.spectrogramBinsDb[static_cast<std::size_t>(i)] =
          mSpectrogramBinsDb[static_cast<std::size_t>(i)].load(std::memory_order_relaxed);
      }
      snapshot.generatedAtMs = mGeneratedAtMs.load(std::memory_order_relaxed);
      return snapshot;
    }

  private:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kMinLinear = 1.0e-9;
    static constexpr double kDbuToDbvOffset = 2.21;
    static constexpr double kPeakAttackSeconds = 0.02;
    static constexpr double kPeakReleaseSeconds = 0.30;
    static constexpr double kRmsAttackSeconds = 0.06;
    static constexpr double kRmsReleaseSeconds = 0.35;

    static double ToDbfs(double linear)
    {
      if (linear <= kMinLinear || !std::isfinite(linear))
        return kSpectrogramMinDbfs;
      return 20.0 * std::log10(linear);
    }

    static double BallisticAlpha(double deltaSeconds, double timeConstantSeconds)
    {
      if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0)
        return 0.0;
      if (!std::isfinite(timeConstantSeconds) || timeConstantSeconds <= 0.0)
        return 0.0;
      return std::exp(-deltaSeconds / timeConstantSeconds);
    }

    static double SmoothWithBallistics(double currentLinear,
                                       double previousLinear,
                                       double deltaSeconds,
                                       double attackSeconds,
                                       double releaseSeconds)
    {
      if (!std::isfinite(currentLinear))
        currentLinear = 0.0;
      if (!std::isfinite(previousLinear))
        previousLinear = 0.0;

      const double target = std::max(0.0, currentLinear);
      const double previous = std::max(0.0, previousLinear);
      const double alpha = BallisticAlpha(
        deltaSeconds,
        target > previous ? attackSeconds : releaseSeconds);
      return alpha * previous + (1.0 - alpha) * target;
    }

    std::atomic<bool> mTelemetryValid{false};
    std::atomic<double> mPeakPercent{0.0};
    std::atomic<double> mRmsPercent{0.0};
    std::atomic<double> mRmsDbu{0.0};
    std::atomic<double> mRmsDbv{0.0};
    std::atomic<double> mRmsVolts{0.0};
    std::array<std::atomic<float>, kSpectrogramBins> mSpectrogramBinsDb{};
    std::atomic<std::uint64_t> mGeneratedAtMs{0};
    double mSmoothedPeakLinear = 0.0;
    double mSmoothedRmsLinear = 0.0;
  };

  inline void RegisterInputAnalyzerEffect()
  {
    EffectTypeInfo info;
    info.type = EffectGuids::kInputAnalyzer;
    info.aliases = {"input_analyzer", "analyzer_input", "audio_analyzer"};
    info.displayName = "Signal Analyzer";
    info.category = "utility";
    info.description = "Live level analyzer with spectrogram";
    info.requiresResource = false;
    info.parameters = {};

    EffectRegistry::Instance().Register(info.type, info, []()
                                        { return std::make_unique<InputAnalyzerEffect>(); });
  }
} // namespace guitarfx
