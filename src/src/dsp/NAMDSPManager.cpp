#include "NAMDSPManager.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

namespace namguitar
{
namespace
{
double DbToLinear(double decibels)
{
  return std::pow(10.0, decibels / 20.0);
}

constexpr int kNumChannels = 2;

} // namespace

NAMDSPManager::NAMDSPManager() = default;
NAMDSPManager::~NAMDSPManager() = default;

void NAMDSPManager::Prepare(double sampleRate, int maxBlockSize)
{
  mSampleRate = sampleRate;
  mMaxBlockSize = std::max(1, maxBlockSize);

  mNamInput.resize(static_cast<std::size_t>(mMaxBlockSize));
  mNamOutput.resize(static_cast<std::size_t>(mMaxBlockSize));
  mIRState.assign(kNumChannels, IRHistory{});
  mGateEnvelope.assign(kNumChannels, 0.0);
  mToneLowState.assign(kNumChannels, 0.0);
  mToneHighState.assign(kNumChannels, 0.0);

  for (auto& model : mModels)
  {
    if (model)
    {
      model->ResetAndPrewarm(mSampleRate, mMaxBlockSize);
    }
  }
}

void NAMDSPManager::Reset()
{
  for (auto& model : mModels)
  {
    if (model)
    {
      model->ResetAndPrewarm(mSampleRate, mMaxBlockSize);
    }
  }
}

bool NAMDSPManager::LoadModel(const std::filesystem::path& modelPath)
{
  try
  {
    for (int channel = 0; channel < kNumChannels; ++channel)
    {
      auto model = nam::get_dsp(modelPath);
      if (!model)
      {
        return false;
      }

      model->ResetAndPrewarm(mSampleRate, mMaxBlockSize);
      mModels[static_cast<std::size_t>(channel)] = std::move(model);
    }

    return true;
  }
  catch (...)
  {
    return false;
  }
}

bool NAMDSPManager::LoadImpulseResponse(const std::filesystem::path& impulsePath)
{
  if (!mIRManager.LoadImpulseResponse(impulsePath, mSampleRate))
  {
    return false;
  }

  mIRState.assign(kNumChannels, IRHistory{});
  for (auto& history : mIRState)
  {
    history.buffer.assign(mIRManager.Impulse().size(), 0.0);
    history.writeIndex = 0;
  }
  return true;
}

void NAMDSPManager::SetInputTrim(double decibels)
{
  mInputTrimDb = decibels;
  mInputTrimLinear = DbToLinear(decibels);
}

void NAMDSPManager::SetOutputTrim(double decibels)
{
  mOutputTrimDb = decibels;
  mOutputTrimLinear = DbToLinear(decibels);
}

void NAMDSPManager::SetDrive(double amount)
{
  mDriveAmount = std::clamp(amount, 0.0, 1.0);
}

void NAMDSPManager::SetTone(double tilt)
{
  mToneTilt = std::clamp(tilt, -1.0, 1.0);
}

void NAMDSPManager::SetGateEnabled(bool enabled)
{
  mGateEnabled = enabled;
}

void NAMDSPManager::SetGateThreshold(double decibels)
{
  mGateThreshold = decibels;
}

void NAMDSPManager::Process(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const auto frames = std::min(nFrames, mMaxBlockSize);

  if (!inputs || !outputs)
  {
    return;
  }

  const auto impulseSize = static_cast<int>(mIRManager.Impulse().size());

  for (int channel = 0; channel < kNumChannels; ++channel)
  {
    iplug::sample* inputChannel = inputs[channel];
    iplug::sample* outputChannel = outputs[channel];
    if (!inputChannel || !outputChannel)
    {
      continue;
    }

    std::vector<double> channelBuffer(frames, 0.0);
    for (int frame = 0; frame < frames; ++frame)
    {
      double sample = static_cast<double>(inputChannel[frame]) * mInputTrimLinear;
      sample = ApplyGate(sample, channel);
      sample = ApplyDrive(sample);
      sample = ApplyTone(sample, channel);
  mNamInput[static_cast<std::size_t>(frame)] = static_cast<NAM_SAMPLE>(sample);
    }

    auto& model = mModels[static_cast<std::size_t>(channel)];
    if (model)
    {
      model->process(mNamInput.data(), mNamOutput.data(), frames);
      for (int frame = 0; frame < frames; ++frame)
      {
  channelBuffer[frame] = static_cast<double>(mNamOutput[static_cast<std::size_t>(frame)]);
      }
    }
    else
    {
      for (int frame = 0; frame < frames; ++frame)
      {
        channelBuffer[frame] = static_cast<double>(mNamInput[static_cast<std::size_t>(frame)]);
      }
    }

    if (impulseSize > 0)
    {
      ApplyImpulseResponse(channelBuffer, channel);
    }

    for (int frame = 0; frame < frames; ++frame)
    {
      outputChannel[frame] = static_cast<iplug::sample>(channelBuffer[frame] * mOutputTrimLinear);
    }
  }
}

double NAMDSPManager::ApplyDrive(double sample) const
{
  // Soft clip the signal to emulate preamp saturation before it hits the NAM block.
  const double drive = 1.0 + mDriveAmount * 9.0;
  const double driven = std::tanh(sample * drive);
  return driven;
}

double NAMDSPManager::ApplyTone(double sample, int channel)
{
  // Split the spectrum with a one-pole filter pair so we can tilt towards lows or highs.
  const double cutoff = 400.0;
  const double omega = 2.0 * std::numbers::pi * cutoff / mSampleRate;
  const double alpha = omega / (omega + 1.0);

  const double low = alpha * sample + (1.0 - alpha) * mToneLowState[static_cast<std::size_t>(channel)];
  const double high = sample - low;
  mToneLowState[static_cast<std::size_t>(channel)] = low;
  mToneHighState[static_cast<std::size_t>(channel)] = high;

  const double mix = (mToneTilt + 1.0) * 0.5; // 0..1
  return low * (1.0 - mix) + high * mix;
}

double NAMDSPManager::ApplyGate(double sample, int channel)
{
  if (!mGateEnabled)
  {
    return sample;
  }

  // Track the envelope with simple attack/release smoothing; anything below threshold is muted.
  const double absSample = std::abs(sample);
  const double attack = 0.01;
  const double release = 0.2;
  const double coeff = absSample > mGateEnvelope[static_cast<std::size_t>(channel)] ? attack : release;
  mGateEnvelope[static_cast<std::size_t>(channel)] = coeff * absSample
                                                    + (1.0 - coeff) * mGateEnvelope[static_cast<std::size_t>(channel)];

  const double thresholdLinear = DbToLinear(mGateThreshold);
  if (mGateEnvelope[static_cast<std::size_t>(channel)] < thresholdLinear)
  {
    return 0.0;
  }

  return sample;
}

void NAMDSPManager::ApplyImpulseResponse(std::vector<double>& channelSamples, int channel) const
{
  const auto& impulse = mIRManager.Impulse();
  if (impulse.empty())
  {
    return;
  }

  auto& history = mIRState[static_cast<std::size_t>(channel)];
  if (history.buffer.empty())
  {
    history.buffer.assign(impulse.size(), 0.0);
    history.writeIndex = 0;
  }

  const std::size_t impulseSize = impulse.size();
  for (std::size_t i = 0; i < channelSamples.size(); ++i)
  {
    // Feed the newest sample into the circular buffer and accumulate FIR taps for cabinet coloration.
    history.buffer[history.writeIndex] = channelSamples[i];

    double acc = 0.0;
    std::size_t bufferIndex = history.writeIndex;
    for (std::size_t tap = 0; tap < impulseSize; ++tap)
    {
      acc += history.buffer[bufferIndex] * impulse[tap];
      if (bufferIndex == 0)
      {
        bufferIndex = impulseSize - 1;
      }
      else
      {
        --bufferIndex;
      }
    }

    channelSamples[i] = acc;

    history.writeIndex = (history.writeIndex + 1) % impulseSize;
  }
}

} // namespace namguitar
