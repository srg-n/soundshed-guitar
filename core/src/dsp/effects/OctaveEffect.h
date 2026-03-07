#pragma once

#include "dsp/EffectProcessor.h"
#include "dsp/EffectRegistry.h"
#include "dsp/EffectGuids.h"
#include "signalsmith-stretch.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace guitarfx
{
  /**
   * Octaveeffect with octave up/down blend.
   */
  class OctaveEffect : public EffectProcessor
  {
  public:
    void Prepare(double sampleRate, int maxBlockSize) override
    {
      mSampleRate = sampleRate;
      mMaxBlockSize = maxBlockSize;

      const size_t bufferSize = static_cast<size_t>(std::max(1, maxBlockSize));
      mMonoIn.assign(bufferSize, 0.0f);
      mWetUp.assign(bufferSize, 0.0f);
      mWetDown.assign(bufferSize, 0.0f);

      mUpStretch.presetCheaper(1, static_cast<float>(sampleRate), false);
      mDownStretch.presetCheaper(1, static_cast<float>(sampleRate), false);
      mConfigured = true;
      ApplyStretchSettings();
      UpdateToneCoefficient();
      Reset();
    }

    void Reset() override
    {
      if (mConfigured)
      {
        mUpStretch.reset();
        mDownStretch.reset();
      }

      mToneState = 0.0f;
    }

    void Process(float **inputs, float **outputs, int numSamples) override
    {
      if (!inputs || !outputs)
        return;

      numSamples = std::min(numSamples, mMaxBlockSize);
      if (numSamples <= 0)
        return;

      if (!mConfigured)
      {
        CopyDry(inputs, outputs, numSamples);
        return;
      }

      if (mMix <= 0.0f || (mOctaveUp <= 0.0f && mOctaveDown <= 0.0f))
      {
        CopyDry(inputs, outputs, numSamples);
        return;
      }

      for (int i = 0; i < numSamples; ++i)
      {
        const float inL = inputs[0] ? inputs[0][i] : 0.0f;
        const float inR = inputs[1] ? inputs[1][i] : 0.0f;
        mMonoIn[static_cast<size_t>(i)] = 0.5f * (inL + inR);
      }

      float *inputPtrs[1] = {mMonoIn.data()};
      float *upPtrs[1] = {mWetUp.data()};
      float *downPtrs[1] = {mWetDown.data()};

      mUpStretch.process(inputPtrs, numSamples, upPtrs, numSamples);
      mDownStretch.process(inputPtrs, numSamples, downPtrs, numSamples);

      const float dryMix = 1.0f - mMix;
      const float wetMix = mMix;

      for (int i = 0; i < numSamples; ++i)
      {
        const float dryL = inputs[0] ? inputs[0][i] : 0.0f;
        const float dryR = inputs[1] ? inputs[1][i] : 0.0f;
        float wet = mWetUp[static_cast<size_t>(i)] * mOctaveUp +
                    mWetDown[static_cast<size_t>(i)] * mOctaveDown;
        wet = ApplyTone(wet);

        if (outputs[0])
          outputs[0][i] = dryL * dryMix + wet * wetMix;
        if (outputs[1])
          outputs[1][i] = dryR * dryMix + wet * wetMix;
      }
    }

    void SetParam(const std::string &key, double value) override
    {
      if (key == "octaveUp")
      {
        mOctaveUp = static_cast<float>(std::clamp(value, 0.0, 1.0));
      }
      else if (key == "octaveDown")
      {
        mOctaveDown = static_cast<float>(std::clamp(value, 0.0, 1.0));
      }
      else if (key == "tone")
      {
        mTone = static_cast<float>(std::clamp(value, 0.0, 1.0));
        UpdateToneCoefficient();
      }
      else if (key == "mix")
      {
        mMix = static_cast<float>(std::clamp(value, 0.0, 1.0));
      }
    }

    void SetConfig(const std::string &, const std::string &) override {}

    [[nodiscard]] double GetParam(const std::string &key) const override
    {
      if (key == "octaveUp")
        return mOctaveUp;
      if (key == "octaveDown")
        return mOctaveDown;
      if (key == "tone")
        return mTone;
      if (key == "mix")
        return mMix;
      return 0.0;
    }

    [[nodiscard]] std::string GetType() const override { return "octave"; }
    [[nodiscard]] std::string GetCategory() const override { return "modulation"; }

    [[nodiscard]] int GetLatencySamples() const override
    {
      if (!mConfigured)
        return 0;
      return std::max(static_cast<int>(mUpStretch.inputLatency()),
                      static_cast<int>(mDownStretch.inputLatency()));
    }

  private:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kTonalityLimitHz = 16000.0;

    void CopyDry(float **inputs, float **outputs, int numSamples)
    {
      for (int ch = 0; ch < 2; ++ch)
      {
        if (!outputs[ch])
          continue;

        if (inputs[ch])
        {
          std::copy(inputs[ch], inputs[ch] + numSamples, outputs[ch]);
        }
        else
        {
          std::fill(outputs[ch], outputs[ch] + numSamples, 0.0f);
        }
      }
    }

    void ApplyStretchSettings()
    {
      if (!mConfigured || mSampleRate <= 0.0)
        return;

      const float tonalityLimit = static_cast<float>(kTonalityLimitHz / mSampleRate);
      mUpStretch.setTransposeSemitones(12.0f, tonalityLimit);
      mDownStretch.setTransposeSemitones(-12.0f, tonalityLimit);
    }

    void UpdateToneCoefficient()
    {
      const float minHz = 400.0f;
      const float maxHz = 4000.0f;
      const float cutoff = minHz + (maxHz - minHz) * mTone;
      const float x = static_cast<float>(2.0 * kPi * cutoff / std::max(1.0, mSampleRate));
      mToneCoef = 1.0f - std::exp(-x);
    }

    float ApplyTone(float input)
    {
      mToneState += mToneCoef * (input - mToneState);
      return mToneState;
    }

    double mSampleRate = 48000.0;
    int mMaxBlockSize = 0;
    bool mConfigured = false;

    float mOctaveUp = 0.6f;
    float mOctaveDown = 0.6f;
    float mTone = 0.5f;
    float mMix = 1.0f;

    float mToneCoef = 0.0f;
    float mToneState = 0.0f;

    signalsmith::stretch::SignalsmithStretch<float> mUpStretch;
    signalsmith::stretch::SignalsmithStretch<float> mDownStretch;
    std::vector<float> mMonoIn;
    std::vector<float> mWetUp;
    std::vector<float> mWetDown;
  };

  inline void RegisterOctaveEffect()
  {
    EffectTypeInfo info;
    info.type = EffectGuids::kOctave;
    info.aliases = {"octave"};
    info.displayName = "Octave";
    info.category = "pitch";
    info.description = "High-quality octave up/down blend";
    info.requiresResource = false;
    info.parameters = {
      {"octaveUp", "Oct Up", 0.6, 0.0, 1.0, "amount"},
      {"octaveDown", "Oct Down", 0.6, 0.0, 1.0, "amount"},
      {"tone", "Tone", 0.5, 0.0, 1.0, "amount"},
      {"mix", "Mix", 1.0, 0.0, 1.0, "amount"}
    };

    EffectRegistry::Instance().Register(info.type, info, []()
      { return std::make_unique<OctaveEffect>(); });
  }

} // namespace guitarfx
