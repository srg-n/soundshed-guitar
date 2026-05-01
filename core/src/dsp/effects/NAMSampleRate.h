#pragma once

namespace guitarfx
{

inline constexpr double kDefaultNamModelSampleRate = 48000.0;

inline double ResolveNamModelProcessingSampleRate(double expectedSampleRate, double hostSampleRate)
{
  (void)hostSampleRate;
  return expectedSampleRate > 0.0 ? expectedSampleRate : kDefaultNamModelSampleRate;
}

} // namespace guitarfx