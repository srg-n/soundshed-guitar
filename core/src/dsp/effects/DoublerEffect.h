#pragma once

#include "dsp/EffectProcessor.h"
#include "dsp/EffectRegistry.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace guitarfx
{
  /**
   * Doubler effect — two independently delayed voices (L/R) derived from the
   * mono sum of the input.  Each voice has its own delay time set by
   * `time ± spread/2`, a slow LFO for organic pitch/time variation, and a
   * one-pole low-pass to keep the doubled voices sitting under the dry signal.
   * Using same-polarity mixing (Haas-style) keeps the result mono-compatible.
   */
  class DoublerEffect : public EffectProcessor
  {
  public:
    void Prepare(double sampleRate, int maxBlockSize) override
    {
      mSampleRate   = sampleRate;
      mMaxBlockSize = maxBlockSize;

      // 120 ms headroom covers max time (50ms) + max spread (30ms) + max mod (5ms)
      const size_t maxSamples = static_cast<size_t>(sampleRate * 0.12) + 4;
      mBufferL.assign(maxSamples, 0.0f);
      mBufferR.assign(maxSamples, 0.0f);

      UpdateDelaySamples();
      UpdateFilter();
      Reset();
    }

    void Reset() override
    {
      std::fill(mBufferL.begin(), mBufferL.end(), 0.0f);
      std::fill(mBufferR.begin(), mBufferR.end(), 0.0f);
      mWritePos = 0;
      mLfoPhaseL = 0.0f;
      mLfoPhaseR = 0.5f; // L and R LFOs start 180° apart for maximum width
      mLpStateL  = 0.0f;
      mLpStateR  = 0.0f;
    }

    void Process(float **inputs, float **outputs, int numSamples) override
    {
      if (mBufferL.empty())
        return;

      const size_t bufSize = mBufferL.size();
      const float  wet     = static_cast<float>(mMix);
      const float  dry     = 1.0f - wet;
      const float  lfoInc  = static_cast<float>(mModRate / mSampleRate);
      const float  modAmt  = static_cast<float>(mModDepth * mSampleRate * 0.001);

      for (int i = 0; i < numSamples; ++i)
      {
        const float inL = inputs[0] ? inputs[0][i] : 0.0f;
        const float inR = inputs[1] ? inputs[1][i] : 0.0f;

        // Mono sum feeds both delay lines
        const float monoIn = (inL + inR) * 0.5f;
        mBufferL[mWritePos] = monoIn;
        mBufferR[mWritePos] = monoIn;

        // LFO — sine, per-channel, 180° apart
        const float lfoL = std::sin(mLfoPhaseL * 6.28318530f);
        const float lfoR = std::sin(mLfoPhaseR * 6.28318530f);
        mLfoPhaseL += lfoInc;
        if (mLfoPhaseL >= 1.0f) mLfoPhaseL -= 1.0f;
        mLfoPhaseR += lfoInc;
        if (mLfoPhaseR >= 1.0f) mLfoPhaseR -= 1.0f;

        // Modulated fractional delay per channel
        const double delayL = std::clamp(mDelayL + lfoL * modAmt,
                                         1.0, static_cast<double>(bufSize - 2));
        const double delayR = std::clamp(mDelayR + lfoR * modAmt,
                                         1.0, static_cast<double>(bufSize - 2));

        // Fractional read
        float voiceL = ReadInterp(mBufferL, bufSize, delayL);
        float voiceR = ReadInterp(mBufferR, bufSize, delayR);

        // Low-pass on doubled voices (keeps them tucked under the dry signal)
        voiceL = ApplyLP(mLpStateL, mLpCoeff, voiceL);
        voiceR = ApplyLP(mLpStateR, mLpCoeff, voiceR);

        // Haas-style mix: same polarity — mono-safe
        if (outputs[0])
          outputs[0][i] = inL * dry + voiceL * wet;
        if (outputs[1])
          outputs[1][i] = inR * dry + voiceR * wet;

        mWritePos = (mWritePos + 1) % bufSize;
      }
    }

    void SetParam(const std::string &key, double value) override
    {
      if (key == "time" || key == "timeMs")
      {
        mDelayMs = std::clamp(value, 1.0, 50.0);
        UpdateDelaySamples();
      }
      else if (key == "spread")
      {
        mSpreadMs = std::clamp(value, 0.0, 30.0);
        UpdateDelaySamples();
      }
      else if (key == "mix")
        mMix = std::clamp(value, 0.0, 1.0);
      else if (key == "modRate")
        mModRate = std::clamp(value, 0.0, 5.0);
      else if (key == "modDepth")
        mModDepth = std::clamp(value, 0.0, 5.0);
      else if (key == "highCut")
      {
        mHighCutHz = std::clamp(value, 1000.0, 20000.0);
        UpdateFilter();
      }
    }

    void SetConfig(const std::string &, const std::string &) override {}

    [[nodiscard]] double GetParam(const std::string &key) const override
    {
      if (key == "time" || key == "timeMs") return mDelayMs;
      if (key == "spread")   return mSpreadMs;
      if (key == "mix")      return mMix;
      if (key == "modRate")  return mModRate;
      if (key == "modDepth") return mModDepth;
      if (key == "highCut")  return mHighCutHz;
      return 0.0;
    }

    [[nodiscard]] std::string GetType() const override { return "delay_doubler"; }
    [[nodiscard]] std::string GetCategory() const override { return "delay"; }

  private:
    // Fractional read from circular buffer with linear interpolation.
    [[nodiscard]] float ReadInterp(const std::vector<float> &buf, size_t bufSize, double delay) const
    {
      const size_t intD = static_cast<size_t>(delay);
      const float  frac = static_cast<float>(delay - static_cast<double>(intD));
      const size_t posA = (mWritePos + bufSize - intD)     % bufSize;
      const size_t posB = (mWritePos + bufSize - intD - 1) % bufSize;
      return buf[posA] * (1.0f - frac) + buf[posB] * frac;
    }

    // One-pole low-pass
    static float ApplyLP(float &state, float coeff, float in)
    {
      state = coeff * in + (1.0f - coeff) * state;
      return state;
    }

    void UpdateDelaySamples()
    {
      const double halfSpread = mSpreadMs * 0.5 * mSampleRate * 0.001;
      const double center     = mDelayMs * mSampleRate * 0.001;
      mDelayL = std::max(1.0, center - halfSpread);
      mDelayR = std::max(1.0, center + halfSpread);
      if (!mBufferL.empty())
      {
        const double maxD = static_cast<double>(mBufferL.size() - 2);
        mDelayL = std::min(mDelayL, maxD);
        mDelayR = std::min(mDelayR, maxD);
      }
    }

    void UpdateFilter()
    {
      if (mSampleRate <= 0.0)
        return;
      constexpr double pi2 = 6.28318530717958647;
      mLpCoeff = static_cast<float>(1.0 - std::exp(-pi2 * mHighCutHz / mSampleRate));
    }

    // Delay buffers (one per channel)
    std::vector<float> mBufferL, mBufferR;
    size_t mWritePos = 0;

    // Derived fractional delay times (in samples)
    double mDelayL = 0.0;
    double mDelayR = 0.0;

    // Parameters
    double mDelayMs   = 20.0;    // Center delay time
    double mSpreadMs  = 30.0;    // L/R offset (L = center - spread/2, R = center + spread/2)
    double mMix       = 0.35;
    double mModRate   = 0.0;     // Hz
    double mModDepth  = 0.0;     // ms
    double mHighCutHz = 10000.0; // LP on doubled voices

    // LP filter coefficient & state
    float mLpCoeff  = 1.0f;
    float mLpStateL = 0.0f;
    float mLpStateR = 0.0f;

    // LFO phase per channel (0..1)
    float mLfoPhaseL = 0.0f;
    float mLfoPhaseR = 0.5f;
  };

  inline void RegisterDoublerEffect()
  {
    EffectTypeInfo info;
    info.type        = "delay_doubler";
    info.displayName = "Doubler";
    info.category    = "delay";
    info.description = "Stereo doubler — two modulated delayed voices for organic width and thickness";
    info.requiresResource = false;
    info.parameters = {
        {"time",     "Time",      20.0,    1.0,    50.0,    "ms"},
        {"spread",   "Spread",    30.0,    0.0,    30.0,    "ms"},
        {"mix",      "Mix",       0.35,    0.0,    1.0,     ""},
        {"modRate",  "Mod Rate",  0.0,     0.0,    5.0,     "Hz",  "", true},
        {"modDepth", "Mod Depth", 0.0,     0.0,    5.0,     "ms",  "", true},
        {"highCut",  "High Cut",  10000.0, 1000.0, 20000.0, "Hz",  "", true}};
    EffectRegistry::Instance().Register("delay_doubler", info, []()
                                        { return std::make_unique<DoublerEffect>(); });
  }

} // namespace guitarfx
