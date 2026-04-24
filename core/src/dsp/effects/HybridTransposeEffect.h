#pragma once

#include "dsp/EffectGuids.h"
#include "dsp/EffectProcessor.h"
#include "dsp/EffectRegistry.h"
#include "dsp/effects/StftTransposeEffect.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace guitarfx
{
  namespace detail
  {
    struct HybridTransposeConfig
    {
      int semitones = 0;
      int latencySamples = 0;
      int transientHoldSamples = 0;
      double shiftDepth = 0.0;
    };

    class HybridTransposeChannel
    {
    public:
      void Prepare(double sampleRate,
                   int maxBlockSize,
                   int semitones,
                   double transientHoldMs,
                   double transientAssist,
                   double brightness)
      {
        mSampleRate = sampleRate;
        mMaxBlockSize = maxBlockSize;
        mAttackCoeff = ComputeEnvelopeCoeff(0.5);
        mReleaseCoeff = ComputeEnvelopeCoeff(20.0);
        mToneAlpha = ComputeToneAlpha(1800.0);
        Configure(semitones, transientHoldMs, transientAssist, brightness);
        Reset();
      }

      void Configure(int semitones,
                     double transientHoldMs,
                     double transientAssist,
                     double brightness)
      {
        mTransientAssist = std::clamp(transientAssist, 0.0, 1.0);
        mBrightness = std::clamp(brightness, 0.0, 1.0);
        mConfig = BuildConfig(mSampleRate, mMaxBlockSize, semitones, transientHoldMs);

        const StftTransposeOptions options = BuildPitchOptions();
        if (!mPitchPrepared)
        {
          mPitch.Prepare(mSampleRate, mMaxBlockSize, mConfig.semitones, options);
          mPitchPrepared = true;
        }
        else
        {
          mPitch.Configure(mConfig.semitones, options);
        }

        EnsureBuffers();
      }

      void Reset()
      {
        std::fill(mDelayBuffer.begin(), mDelayBuffer.end(), 0.0f);
        mWritePos = 0;
        mEnvelope = 0.0f;
        mTransientBlend = 0.0f;
        mTransientCountdown = 0;
        mToneLowpass = 0.0f;
        if (mPitchPrepared)
          mPitch.Reset();
      }

      void Process(const float *input, float *wet, float *dryAligned, int numSamples)
      {
        if (!input || !wet || !dryAligned || mDelayBuffer.empty() || numSamples <= 0)
          return;

        if (static_cast<size_t>(numSamples) > mPitchBuffer.size())
          mPitchBuffer.resize(static_cast<size_t>(numSamples), 0.0f);

        if (mConfig.semitones != 0 && mPitchPrepared)
          mPitch.Process(input, mPitchBuffer.data(), numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
          const float in = input[i];
          mDelayBuffer[mWritePos] = in;

          const float alignedDry = ReadDelay(static_cast<double>(mConfig.latencySamples));
          dryAligned[i] = alignedDry;

          if (mConfig.semitones == 0)
          {
            wet[i] = alignedDry;
            AdvanceWrite();
            continue;
          }

          const float pitched = mPitchBuffer[static_cast<size_t>(i)];
          UpdateTransientState(std::abs(in));

          mToneLowpass += mToneAlpha * (pitched - mToneLowpass);
          const float compensated = pitched + (pitched - mToneLowpass)
            * static_cast<float>(mBrightness * 0.45 * mConfig.shiftDepth);

          const float assist = static_cast<float>(mTransientAssist * mTransientBlend);
          wet[i] = compensated * (1.0f - assist) + alignedDry * assist;

          AdvanceWrite();
        }
      }

      [[nodiscard]] int GetLatencySamples() const
      {
        return mConfig.semitones == 0 ? 0 : mConfig.latencySamples;
      }

    private:
      [[nodiscard]] static HybridTransposeConfig BuildConfig(double sampleRate,
                                                             int maxBlockSize,
                                                             int semitones,
                                                             double transientHoldMs)
      {
        HybridTransposeConfig config;
        config.semitones = std::clamp(semitones, -15, 0);
        if (sampleRate <= 0.0 || maxBlockSize <= 0 || config.semitones == 0)
          return config;

        config.shiftDepth = std::clamp(-static_cast<double>(config.semitones) / 15.0, 0.0, 1.0);
        config.latencySamples = StftTransposeChannel::GetExpectedLatencySamples(config.semitones, 0);
        config.transientHoldSamples = std::max(1,
                                               static_cast<int>(std::lround(sampleRate * std::clamp(transientHoldMs, 2.0, 40.0) * 0.001)));
        return config;
      }

      [[nodiscard]] StftTransposeOptions BuildPitchOptions() const
      {
        StftTransposeOptions options;
        options.mode = 0;
        options.quefrencySeconds = 0.0;
        options.timbre = 1.0;
        options.normalization = true;
        return options;
      }

      void EnsureBuffers()
      {
        const size_t bufferSize = static_cast<size_t>(std::max(mConfig.latencySamples + mMaxBlockSize + 8, 4096));
        mDelayBuffer.assign(bufferSize, 0.0f);
        mPitchBuffer.assign(static_cast<size_t>(std::max(1, mMaxBlockSize)), 0.0f);
      }

      [[nodiscard]] float ReadDelay(double delaySamples) const
      {
        return ReadPosition(static_cast<double>(mWritePos) - delaySamples);
      }

      [[nodiscard]] float ReadPosition(double readPos) const
      {
        if (mDelayBuffer.empty())
          return 0.0f;

        const double wrapped = WrapBufferPosition(readPos);

        const size_t index0 = static_cast<size_t>(wrapped);
        const size_t index1 = (index0 + 1) % mDelayBuffer.size();
        const float frac = static_cast<float>(wrapped - static_cast<double>(index0));
        const float sample0 = mDelayBuffer[index0];
        const float sample1 = mDelayBuffer[index1];
        return sample0 + (sample1 - sample0) * frac;
      }

      void UpdateTransientState(float absInput)
      {
        const float previousEnvelope = mEnvelope;
        const float coeff = absInput > previousEnvelope ? mAttackCoeff : mReleaseCoeff;
        mEnvelope = absInput + coeff * (previousEnvelope - absInput);

        if (mEnvelope > kTransientFloor && mEnvelope > previousEnvelope * kTransientTriggerRatio)
          mTransientCountdown = mConfig.transientHoldSamples;

        const float target = mTransientCountdown > 0 ? 1.0f : 0.0f;
        const float slew = target > mTransientBlend ? 0.30f : 0.05f;
        mTransientBlend += (target - mTransientBlend) * slew;

        if (mTransientCountdown > 0)
          --mTransientCountdown;
      }

      void AdvanceWrite()
      {
        ++mWritePos;
        if (mWritePos >= mDelayBuffer.size())
          mWritePos = 0;
      }

      [[nodiscard]] double WrapBufferPosition(double position) const
      {
        if (mDelayBuffer.empty())
          return 0.0;

        const double size = static_cast<double>(mDelayBuffer.size());
        while (position < 0.0)
          position += size;
        while (position >= size)
          position -= size;
        return position;
      }

      [[nodiscard]] float ComputeEnvelopeCoeff(double timeMs) const
      {
        const double seconds = std::max(1.0e-6, timeMs * 0.001);
        return static_cast<float>(std::exp(-1.0 / (seconds * std::max(1.0, mSampleRate))));
      }

      [[nodiscard]] float ComputeToneAlpha(double cutoffHz) const
      {
        const double clampedCutoff = std::clamp(cutoffHz, 20.0, 0.45 * mSampleRate);
        return static_cast<float>(1.0 - std::exp(-2.0 * 3.14159265358979323846 * clampedCutoff / std::max(1.0, mSampleRate)));
      }

      static constexpr float kTransientFloor = 0.01f;
      static constexpr float kTransientTriggerRatio = 1.6f;

      double mSampleRate = 44100.0;
      int mMaxBlockSize = 512;
      double mTransientAssist = 0.65;
      double mBrightness = 0.35;
      HybridTransposeConfig mConfig;

      StftTransposeChannel mPitch;
      bool mPitchPrepared = false;
      std::vector<float> mDelayBuffer;
      std::vector<float> mPitchBuffer;
      size_t mWritePos = 0;
      float mEnvelope = 0.0f;
      float mTransientBlend = 0.0f;
      int mTransientCountdown = 0;
      float mToneLowpass = 0.0f;

      float mAttackCoeff = 0.0f;
      float mReleaseCoeff = 0.0f;
      float mToneAlpha = 0.0f;
    };
  } // namespace detail

  class HybridTransposeEffect : public EffectProcessor
  {
  public:
    void Prepare(double sampleRate, int maxBlockSize) override
    {
      if (!ValidatePrepare(sampleRate, maxBlockSize))
        return;

      mSampleRate = sampleRate;
      mMaxBlockSize = maxBlockSize;
      mWetL.assign(static_cast<size_t>(maxBlockSize), 0.0f);
      mWetR.assign(static_cast<size_t>(maxBlockSize), 0.0f);
      mDryL.assign(static_cast<size_t>(maxBlockSize), 0.0f);
      mDryR.assign(static_cast<size_t>(maxBlockSize), 0.0f);
      mZero.assign(static_cast<size_t>(maxBlockSize), 0.0f);

      mLeft.Prepare(sampleRate, maxBlockSize, mSemitones, mTransientHoldMs, mTransientAssist, mBrightness);
      mRight.Prepare(sampleRate, maxBlockSize, mSemitones, mTransientHoldMs, mTransientAssist, mBrightness);
      mConfigured = true;
      Reset();
    }

    void Reset() override
    {
      if (!mConfigured)
        return;

      mLeft.Reset();
      mRight.Reset();
    }

    void Process(float **inputs, float **outputs, int numSamples) override
    {
      if (!inputs || !outputs || numSamples <= 0)
        return;

      if (!mConfigured)
        return;

      if (mSemitones == 0)
      {
        for (int ch = 0; ch < 2; ++ch)
        {
          if (outputs[ch])
          {
            const float *source = inputs[ch] ? inputs[ch] : mZero.data();
            std::copy(source, source + numSamples, outputs[ch]);
          }
        }
        return;
      }

      if (static_cast<size_t>(numSamples) > mWetL.size())
      {
        mWetL.resize(static_cast<size_t>(numSamples), 0.0f);
        mWetR.resize(static_cast<size_t>(numSamples), 0.0f);
        mDryL.resize(static_cast<size_t>(numSamples), 0.0f);
        mDryR.resize(static_cast<size_t>(numSamples), 0.0f);
        mZero.resize(static_cast<size_t>(numSamples), 0.0f);
      }

      const float *leftInput = inputs[0] ? inputs[0] : mZero.data();
      const float *rightInput = inputs[1] ? inputs[1] : mZero.data();

      mLeft.Process(leftInput, mWetL.data(), mDryL.data(), numSamples);
      mRight.Process(rightInput, mWetR.data(), mDryR.data(), numSamples);

      const float dryMix = static_cast<float>(1.0 - mMix);
      const float wetMix = static_cast<float>(mMix);
      for (int i = 0; i < numSamples; ++i)
      {
        if (outputs[0])
          outputs[0][i] = mDryL[static_cast<size_t>(i)] * dryMix + mWetL[static_cast<size_t>(i)] * wetMix;
        if (outputs[1])
          outputs[1][i] = mDryR[static_cast<size_t>(i)] * dryMix + mWetR[static_cast<size_t>(i)] * wetMix;
      }
    }

    void SetParam(const std::string &key, double value) override
    {
      if (key == "semitones")
      {
        const int semitones = static_cast<int>(std::round(std::clamp(value, -15.0, 0.0)));
        if (semitones != mSemitones)
        {
          mSemitones = semitones;
          ReconfigureChannels();
        }
      }
      else if (key == "mix")
      {
        mMix = std::clamp(value, 0.0, 1.0);
      }
      else if (key == "transientAssist")
      {
        mTransientAssist = std::clamp(value, 0.0, 1.0);
        ReconfigureChannels();
      }
      else if (key == "transientHoldMs")
      {
        mTransientHoldMs = std::clamp(value, 2.0, 40.0);
        ReconfigureChannels();
      }
      else if (key == "brightness")
      {
        mBrightness = std::clamp(value, 0.0, 1.0);
        ReconfigureChannels();
      }
    }

    void SetConfig(const std::string &, const std::string &) override {}

    [[nodiscard]] double GetParam(const std::string &key) const override
    {
      if (key == "semitones")
        return static_cast<double>(mSemitones);
      if (key == "mix")
        return mMix;
      if (key == "transientAssist")
        return mTransientAssist;
      if (key == "transientHoldMs")
        return mTransientHoldMs;
      if (key == "brightness")
        return mBrightness;
      return 0.0;
    }

    [[nodiscard]] std::string GetType() const override { return "transpose_hybrid"; }
    [[nodiscard]] std::string GetCategory() const override { return "pitch"; }

    [[nodiscard]] int GetLatencySamples() const override
    {
      return mConfigured ? mLeft.GetLatencySamples() : 0;
    }

  private:
    void ReconfigureChannels()
    {
      if (!mConfigured)
        return;

      mLeft.Configure(mSemitones, mTransientHoldMs, mTransientAssist, mBrightness);
      mRight.Configure(mSemitones, mTransientHoldMs, mTransientAssist, mBrightness);
    }

    int mSemitones = 0;
    double mMix = 1.0;
    double mTransientAssist = 0.65;
    double mTransientHoldMs = 12.0;
    double mBrightness = 0.35;
    bool mConfigured = false;

    detail::HybridTransposeChannel mLeft;
    detail::HybridTransposeChannel mRight;
    std::vector<float> mWetL;
    std::vector<float> mWetR;
    std::vector<float> mDryL;
    std::vector<float> mDryR;
    std::vector<float> mZero;
  };

  inline void RegisterHybridTransposeEffect()
  {
    EffectTypeInfo info;
    info.type = EffectGuids::kTransposeHybrid;
    info.aliases = {"transpose_hybrid"};
    info.displayName = "Transpose (Hybrid)";
    info.category = "pitch";
    info.description = "Low-latency downshift transpose with transient-assisted dry alignment";
    info.requiresResource = false;
    info.parameters = {
      {"semitones", "Semitones", -5.0, -15.0, 0.0, "st", "", false, 1.0},
      {"mix", "Mix", 1.0, 0.0, 1.0, "amount"},
      {"transientAssist", "Transient Assist", 0.65, 0.0, 1.0, "amount", "", true, 0.01},
      {"transientHoldMs", "Transient Hold", 12.0, 2.0, 40.0, "ms", "", true, 0.5},
      {"brightness", "Brightness", 0.35, 0.0, 1.0, "amount", "", true, 0.01}
    };
    EffectRegistry::Instance().Register(info.type, info, []()
                                        { return std::make_unique<HybridTransposeEffect>(); });
  }

} // namespace guitarfx