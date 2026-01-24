#pragma once

#include "dsp/EffectProcessor.h"
#include "dsp/EffectRegistry.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace guitarfx
{
  /**
   * Simple pitch shift effect using linear interpolation on a circular buffer.
   * Shifts pitch by semitones (-24 to +24).
   */
  class PitchShiftEffect : public EffectProcessor
  {
  public:
    void Prepare(double sampleRate, int maxBlockSize) override
    {
      mSampleRate = sampleRate;
      mMaxBlockSize = maxBlockSize;

        // Allocate circular buffers (default ~180ms of audio)
      const size_t bufferSize = static_cast<size_t>(
          std::max(1.0, sampleRate * (kDefaultBufferMs / 1000.0)));
      mBufferL.resize(bufferSize, 0.0f);
      mBufferR.resize(bufferSize, 0.0f);
      mShiftedL.resize(static_cast<size_t>(maxBlockSize), 0.0f);
      mShiftedR.resize(static_cast<size_t>(maxBlockSize), 0.0f);

        mDelaySamples = static_cast<std::size_t>(std::clamp(
          sampleRate * (kDefaultDelayMs / 1000.0),
          1.0,
          static_cast<double>(bufferSize > 1 ? bufferSize - 1 : 1)));

      UpdatePitchRatio();
      Reset();
    }

    void Reset() override
    {
      std::fill(mBufferL.begin(), mBufferL.end(), 0.0f);
      std::fill(mBufferR.begin(), mBufferR.end(), 0.0f);
      mWriteIndex = 0;
      if (!mBufferL.empty())
      {
        const std::size_t bufSize = mBufferL.size();
        const std::size_t startIndex = (bufSize + mWriteIndex - mDelaySamples) % bufSize;
        mReadPhaseA = static_cast<double>(startIndex);
        mReadPhaseB = std::fmod(mReadPhaseA + static_cast<double>(bufSize) * 0.5,
                                static_cast<double>(bufSize));
      }
      else
      {
        mReadPhaseA = 0.0;
        mReadPhaseB = 0.0;
      }
    }

    void Process(float **inputs, float **outputs, int numSamples) override
    {
      // If no pitch shift, pass through
      if (mSemitones == 0)
      {
        for (int ch = 0; ch < 2; ++ch)
        {
          if (inputs[ch] && outputs[ch])
          {
            std::copy(inputs[ch], inputs[ch] + numSamples, outputs[ch]);
          }
        }
        return;
      }

      const std::size_t bufSize = mBufferL.size();
      if (bufSize == 0)
        return;

      // Write input to circular buffer
      for (int i = 0; i < numSamples; ++i)
      {
        mBufferL[mWriteIndex] = inputs[0] ? inputs[0][i] : 0.0f;
        mBufferR[mWriteIndex] = inputs[1] ? inputs[1][i] : 0.0f;
        mWriteIndex = (mWriteIndex + 1) % bufSize;
      }

      // Read at shifted rate with linear interpolation
      for (int i = 0; i < numSamples; ++i)
      {
        const double readIdxA = std::fmod(mReadPhaseA, static_cast<double>(bufSize));
        const double readIdxB = std::fmod(mReadPhaseB, static_cast<double>(bufSize));

        const std::size_t a0 = static_cast<std::size_t>(readIdxA);
        const std::size_t a1 = (a0 + 1) % bufSize;
        const double aFrac = readIdxA - static_cast<double>(a0);

        const std::size_t b0 = static_cast<std::size_t>(readIdxB);
        const std::size_t b1 = (b0 + 1) % bufSize;
        const double bFrac = readIdxB - static_cast<double>(b0);

        const double phaseA = readIdxA / static_cast<double>(bufSize);
        const double phaseB = readIdxB / static_cast<double>(bufSize);
        const double wA = 0.5 - 0.5 * std::cos(kTwoPi * phaseA);
        const double wB = 0.5 - 0.5 * std::cos(kTwoPi * phaseB);
        const double wSum = wA + wB;
        const double wAn = wSum > 0.0 ? (wA / wSum) : 0.5;
        const double wBn = wSum > 0.0 ? (wB / wSum) : 0.5;

        const float aL = static_cast<float>(mBufferL[a0] * (1.0 - aFrac) + mBufferL[a1] * aFrac);
        const float aR = static_cast<float>(mBufferR[a0] * (1.0 - aFrac) + mBufferR[a1] * aFrac);
        const float bL = static_cast<float>(mBufferL[b0] * (1.0 - bFrac) + mBufferL[b1] * bFrac);
        const float bR = static_cast<float>(mBufferR[b0] * (1.0 - bFrac) + mBufferR[b1] * bFrac);

        mShiftedL[static_cast<std::size_t>(i)] = static_cast<float>(aL * wAn + bL * wBn);
        mShiftedR[static_cast<std::size_t>(i)] = static_cast<float>(aR * wAn + bR * wBn);

        // Advance read phase at pitch-shifted rate
        mReadPhaseA = std::fmod(mReadPhaseA + mPitchRatio, static_cast<double>(bufSize));
        mReadPhaseB = std::fmod(mReadPhaseB + mPitchRatio, static_cast<double>(bufSize));
      }

      // Copy to output
      if (outputs[0])
        std::copy(mShiftedL.begin(), mShiftedL.begin() + numSamples, outputs[0]);
      if (outputs[1])
        std::copy(mShiftedR.begin(), mShiftedR.begin() + numSamples, outputs[1]);
    }

    void SetParam(const std::string &key, double value) override
    {
      if (key == "semitones")
      {
        mSemitones = static_cast<int>(std::clamp(value, -24.0, 24.0));
        UpdatePitchRatio();
      }
      else if (key == "mix")
      {
        mMix = std::clamp(value, 0.0, 1.0);
      }
    }

    void SetConfig(const std::string &, const std::string &) override {}

    [[nodiscard]] double GetParam(const std::string &key) const override
    {
      if (key == "semitones")
        return static_cast<double>(mSemitones);
      if (key == "mix")
        return mMix;
      return 0.0;
    }

    [[nodiscard]] std::string GetType() const override { return "pitch_shift"; }
    [[nodiscard]] std::string GetCategory() const override { return "modulation"; }

  private:
    void UpdatePitchRatio()
    {
      // Convert semitones to pitch ratio: ratio = 2^(semitones/12)
      mPitchRatio = std::pow(2.0, static_cast<double>(mSemitones) / 12.0);
    }

    int mSemitones = 0;
    double mMix = 1.0;
    double mPitchRatio = 1.0;

    static constexpr double kDefaultBufferMs = 180.0;
    static constexpr double kDefaultDelayMs = 30.0;
    static constexpr double kTwoPi = 6.283185307179586;

    std::vector<float> mBufferL, mBufferR;
    std::vector<float> mShiftedL, mShiftedR;
    std::size_t mWriteIndex = 0;
    std::size_t mDelaySamples = 0;
    double mReadPhaseA = 0.0;
    double mReadPhaseB = 0.0;
  };

  inline void RegisterPitchShiftEffect()
  {
    EffectTypeInfo info;
    info.type = "pitch_shift";
    info.displayName = "Pitch Shift";
    info.category = "modulation";
    info.description = "Simple pitch shift effect for transpose up/down by semitones";
    info.requiresResource = false;
    info.parameters = {
        {"semitones", "Semitones", 0.0, -24.0, 24.0, "st"},
        {"mix", "Mix", 1.0, 0.0, 1.0, "%"}};
    EffectRegistry::Instance().Register("pitch_shift", info, []()
                                        { return std::make_unique<PitchShiftEffect>(); });
  }

} // namespace guitarfx
