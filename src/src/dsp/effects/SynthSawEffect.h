#pragma once

#include "dsp/EffectProcessor.h"
#include "dsp/EffectRegistry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace guitarfx
{
  /**
   * Synthesizer effect that converts audio to a sawtooth wave.
   * Uses YIN pitch detection to track the fundamental frequency
   * and generates a monophonic sawtooth oscillator following the detected pitch.
   * 
   * Future: Can expose detected pitch as MIDI note data.
   */
  class SynthSawEffect : public EffectProcessor
  {
  public:
    void Prepare(double sampleRate, int maxBlockSize) override
    {
      mSampleRate = sampleRate;
      mMaxBlockSize = maxBlockSize;

      // YIN buffer: needs enough samples for low frequencies (~50 Hz min)
      // At 44100 Hz, 50 Hz period = ~882 samples, need 2x for YIN = ~1764
      mYinBufferSize = static_cast<size_t>(mSampleRate / kMinFrequency);
      mYinBuffer.assign(mYinBufferSize, 0.0f);
      mYinDiff.assign(mYinBufferSize / 2, 0.0f);
      mYinCumulative.assign(mYinBufferSize / 2, 0.0f);

      mInputBuffer.assign(mYinBufferSize * 2, 0.0f);
      mInputWritePos = 0;
      mSamplesCollected = 0;

      Reset();
    }

    void Reset() override
    {
      mOscPhase = 0.0;
      mCurrentFreq = 0.0;
      mTargetFreq = 0.0;
      mEnvelopeLevel = 0.0f;
      mPitchConfidence = 0.0f;
      mInputWritePos = 0;
      mSamplesCollected = 0;
      std::fill(mInputBuffer.begin(), mInputBuffer.end(), 0.0f);
      std::fill(mYinBuffer.begin(), mYinBuffer.end(), 0.0f);
    }

    void Process(float **inputs, float **outputs, int numSamples) override
    {
      if (!inputs || !outputs)
        return;

      for (int i = 0; i < numSamples; ++i)
      {
        // Mix to mono for pitch detection
        const float inL = inputs[0] ? inputs[0][i] : 0.0f;
        const float inR = inputs[1] ? inputs[1][i] : 0.0f;
        const float mono = 0.5f * (inL + inR);

        // Add to input buffer for pitch detection
        mInputBuffer[mInputWritePos] = mono;
        mInputWritePos = (mInputWritePos + 1) % mInputBuffer.size();
        mSamplesCollected++;

        // Run pitch detection every hop size samples
        if (mSamplesCollected >= kHopSize)
        {
          mSamplesCollected = 0;
          DetectPitch();
        }

        // Envelope follower on input signal
        const float absInput = std::abs(mono);
        if (absInput > mEnvelopeLevel)
        {
          // Attack
          mEnvelopeLevel += mAttackCoef * (absInput - mEnvelopeLevel);
        }
        else
        {
          // Release
          mEnvelopeLevel += mReleaseCoef * (absInput - mEnvelopeLevel);
        }

        // Smooth frequency transition
        if (mTargetFreq > 0.0 && mPitchConfidence > kConfidenceThreshold)
        {
          const double freqDiff = mTargetFreq - mCurrentFreq;
          mCurrentFreq += mGlideCoef * freqDiff;
        }

        // Generate sawtooth waveform
        float synthOut = 0.0f;
        if (mCurrentFreq > kMinFrequency && mEnvelopeLevel > kGateThreshold)
        {
          // Apply octave shift
          double freq = mCurrentFreq * std::pow(2.0, mOctaveShift);
          
          // Apply detune in cents
          freq *= std::pow(2.0, mDetune / 1200.0);

          // Clamp frequency to reasonable range
          freq = std::clamp(freq, kMinFrequency, kMaxFrequency);

          // Phase increment
          const double phaseInc = freq / mSampleRate;
          mOscPhase += phaseInc;
          if (mOscPhase >= 1.0)
            mOscPhase -= 1.0;

          // Sawtooth: ramps from -1 to +1
          synthOut = static_cast<float>(2.0 * mOscPhase - 1.0);

          // Apply envelope
          synthOut *= mEnvelopeLevel;
        }

        // Apply output gain
        synthOut *= mOutputGain;

        // Mix dry/wet
        const float dryMix = 1.0f - mMix;
        const float wetMix = mMix;
        const float outL = inL * dryMix + synthOut * wetMix;
        const float outR = inR * dryMix + synthOut * wetMix;

        if (outputs[0])
          outputs[0][i] = outL;
        if (outputs[1])
          outputs[1][i] = outR;
      }
    }

    void SetParam(const std::string &key, double value) override
    {
      if (key == "mix")
      {
        mMix = static_cast<float>(std::clamp(value, 0.0, 1.0));
      }
      else if (key == "attack")
      {
        mAttackMs = std::clamp(value, 0.1, 100.0);
        UpdateEnvelopeCoefs();
      }
      else if (key == "release")
      {
        mReleaseMs = std::clamp(value, 10.0, 1000.0);
        UpdateEnvelopeCoefs();
      }
      else if (key == "detune")
      {
        mDetune = std::clamp(value, -100.0, 100.0);
      }
      else if (key == "octaveShift")
      {
        mOctaveShift = std::clamp(value, -2.0, 2.0);
      }
      else if (key == "glide")
      {
        mGlideMs = std::clamp(value, 0.0, 500.0);
        UpdateGlideCoef();
      }
      else if (key == "outputGain")
      {
        const double dB = std::clamp(value, -24.0, 12.0);
        mOutputGain = static_cast<float>(std::pow(10.0, dB / 20.0));
      }
      else if (key == "gate")
      {
        const double dB = std::clamp(value, -80.0, 0.0);
        kGateThreshold = static_cast<float>(std::pow(10.0, dB / 20.0));
      }
    }

    void SetConfig(const std::string &, const std::string &) override {}

    [[nodiscard]] double GetParam(const std::string &key) const override
    {
      if (key == "mix")
        return mMix;
      if (key == "attack")
        return mAttackMs;
      if (key == "release")
        return mReleaseMs;
      if (key == "detune")
        return mDetune;
      if (key == "octaveShift")
        return mOctaveShift;
      if (key == "glide")
        return mGlideMs;
      if (key == "outputGain")
        return 20.0 * std::log10(mOutputGain + 1e-10f);
      if (key == "gate")
        return 20.0 * std::log10(kGateThreshold + 1e-10f);
      return 0.0;
    }

    [[nodiscard]] std::string GetType() const override { return "synth_saw"; }
    [[nodiscard]] std::string GetCategory() const override { return "synth"; }

    // For future MIDI output support
    [[nodiscard]] double GetDetectedFrequency() const { return mCurrentFreq; }
    [[nodiscard]] double GetDetectedMidiNote() const
    {
      if (mCurrentFreq <= 0.0)
        return -1.0;
      // MIDI note = 69 + 12 * log2(freq / 440)
      return 69.0 + 12.0 * std::log2(mCurrentFreq / 440.0);
    }
    [[nodiscard]] float GetPitchConfidence() const { return mPitchConfidence; }

  private:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kMinFrequency = 50.0;   // ~G1
    static constexpr double kMaxFrequency = 2000.0; // ~B6
    static constexpr float kConfidenceThreshold = 0.8f;
    static constexpr size_t kHopSize = 128; // Pitch detection hop size

    // YIN algorithm threshold
    static constexpr float kYinThreshold = 0.15f;

    void UpdateEnvelopeCoefs()
    {
      // Time constant: 1 - exp(-1 / (time_ms * sampleRate / 1000))
      const double attackSamples = mAttackMs * mSampleRate / 1000.0;
      const double releaseSamples = mReleaseMs * mSampleRate / 1000.0;
      mAttackCoef = static_cast<float>(1.0 - std::exp(-1.0 / std::max(1.0, attackSamples)));
      mReleaseCoef = static_cast<float>(1.0 - std::exp(-1.0 / std::max(1.0, releaseSamples)));
    }

    void UpdateGlideCoef()
    {
      const double glideSamples = mGlideMs * mSampleRate / 1000.0;
      mGlideCoef = glideSamples > 0.0 ? (1.0 - std::exp(-1.0 / glideSamples)) : 1.0;
    }

    /**
     * YIN pitch detection algorithm.
     * Estimates fundamental frequency from the input buffer.
     */
    void DetectPitch()
    {
      const size_t halfSize = mYinBufferSize / 2;

      // Copy recent samples to YIN buffer (unwrap circular buffer)
      for (size_t i = 0; i < mYinBufferSize; ++i)
      {
        size_t idx = (mInputWritePos + mInputBuffer.size() - mYinBufferSize + i) % mInputBuffer.size();
        mYinBuffer[i] = mInputBuffer[idx];
      }

      // Step 1: Difference function
      for (size_t tau = 0; tau < halfSize; ++tau)
      {
        float sum = 0.0f;
        for (size_t j = 0; j < halfSize; ++j)
        {
          const float diff = mYinBuffer[j] - mYinBuffer[j + tau];
          sum += diff * diff;
        }
        mYinDiff[tau] = sum;
      }

      // Step 2: Cumulative mean normalized difference
      mYinCumulative[0] = 1.0f;
      float runningSum = 0.0f;
      for (size_t tau = 1; tau < halfSize; ++tau)
      {
        runningSum += mYinDiff[tau];
        mYinCumulative[tau] = mYinDiff[tau] * tau / (runningSum + 1e-10f);
      }

      // Step 3: Absolute threshold
      size_t tauEstimate = 0;
      for (size_t tau = 2; tau < halfSize; ++tau)
      {
        if (mYinCumulative[tau] < kYinThreshold)
        {
          // Find local minimum
          while (tau + 1 < halfSize && mYinCumulative[tau + 1] < mYinCumulative[tau])
          {
            ++tau;
          }
          tauEstimate = tau;
          break;
        }
      }

      // Step 4: Parabolic interpolation for better accuracy
      if (tauEstimate > 0 && tauEstimate < halfSize - 1)
      {
        const float s0 = mYinCumulative[tauEstimate - 1];
        const float s1 = mYinCumulative[tauEstimate];
        const float s2 = mYinCumulative[tauEstimate + 1];
        
        // Parabolic interpolation
        const float delta = (s2 - s0) / (2.0f * (2.0f * s1 - s0 - s2) + 1e-10f);
        const float refinedTau = static_cast<float>(tauEstimate) + delta;
        
        if (refinedTau > 0.0f)
        {
          mTargetFreq = mSampleRate / refinedTau;
          mPitchConfidence = 1.0f - mYinCumulative[tauEstimate];
          
          // Clamp to valid range
          if (mTargetFreq < kMinFrequency || mTargetFreq > kMaxFrequency)
          {
            mTargetFreq = 0.0;
            mPitchConfidence = 0.0f;
          }
        }
        else
        {
          mTargetFreq = 0.0;
          mPitchConfidence = 0.0f;
        }
      }
      else
      {
        // No valid pitch detected
        mTargetFreq = 0.0;
        mPitchConfidence = 0.0f;
      }
    }

    // Parameters
    float mMix = 1.0f;
    double mAttackMs = 5.0;
    double mReleaseMs = 100.0;
    double mDetune = 0.0;       // cents
    double mOctaveShift = 0.0;  // octaves (-2 to +2)
    double mGlideMs = 20.0;     // portamento time
    float mOutputGain = 1.0f;
    float kGateThreshold = 0.001f; // -60 dB default

    // Envelope follower
    float mAttackCoef = 0.1f;
    float mReleaseCoef = 0.01f;
    float mEnvelopeLevel = 0.0f;

    // Oscillator state
    double mOscPhase = 0.0;
    double mCurrentFreq = 0.0;
    double mTargetFreq = 0.0;
    double mGlideCoef = 0.1;

    // Pitch detection
    float mPitchConfidence = 0.0f;
    size_t mYinBufferSize = 2048;
    std::vector<float> mYinBuffer;
    std::vector<float> mYinDiff;
    std::vector<float> mYinCumulative;

    // Input circular buffer
    std::vector<float> mInputBuffer;
    size_t mInputWritePos = 0;
    size_t mSamplesCollected = 0;
  };

  inline void RegisterSynthSawEffect()
  {
    EffectTypeInfo info;
    info.type = "synth_saw";
    info.displayName = "Synth Saw";
    info.category = "synth";
    info.description = "Converts audio to sawtooth synth wave via pitch tracking";
    info.requiresResource = false;
    info.parameters = {
      {"mix", "Mix", 1.0, 0.0, 1.0, "amount"},
      {"attack", "Attack", 5.0, 0.1, 100.0, "ms"},
      {"release", "Release", 100.0, 10.0, 1000.0, "ms"},
      {"detune", "Detune", 0.0, -100.0, 100.0, "cents"},
      {"octaveShift", "Octave", 0.0, -2.0, 2.0, "oct"},
      {"glide", "Glide", 20.0, 0.0, 500.0, "ms"},
      {"outputGain", "Output", 0.0, -24.0, 12.0, "dB"},
      {"gate", "Gate", -60.0, -80.0, 0.0, "dB"}
    };

    EffectRegistry::Instance().Register("synth_saw", info, []()
      { return std::make_unique<SynthSawEffect>(); });
  }

} // namespace guitarfx
