# DSP Effects Review & Improvement Plan

> Reviewed: 2026-02-25  
> Scope: `core/dsp/effects/` — 31 effects across 8 categories

---

## Summary

The effects codebase demonstrates solid DSP fundamentals with well-organized architecture, proper filter design, and good documentation coverage. However, several gaps exist before the codebase is production-grade for professional DAW integration.

---

## Critical Issues 🔴

### 1. No Thread Safety in Parameter Updates

All effects store parameters in plain member variables (`mDrive`, `mTone`, etc.). `SetParam()` is called from the UI thread while `Process()` runs on the audio thread — classic data race that can cause audible glitches or crashes.

**Fix:** Use `std::atomic<float>` for all parameter members, or snapshot parameters into a local struct at the top of each `Process()` call.

---

### 2. Missing `Prepare()` Input Validation

Several effects divide by `mSampleRate` without guarding against `sampleRate <= 0`, risking NaN / divide-by-zero propagation through the entire signal graph.

**Fix:** Add a guard in the base class `Prepare()`:
```cpp
if (sampleRate <= 0 || blockSize <= 0) return; // or assert/throw
```

---

### 3. Silent Resource Loading Failures

NAM model and IR loading failures are logged but not surfaced to callers or the UI. Signal chains silently bypass missing models with no user feedback.

**Fix:** Return a structured `Result` / error code from `LoadResource()` and relay failures to the UI via the messaging layer.

---

## Medium Priority Issues 🟡

### 4. No Smooth Parameter Transitions

Rate/depth changes on modulation effects (chorus, flanger, phaser) are instantaneous, causing audible clicks. This applies to all parameters that feed into per-sample calculations.

**Fix:** Add a one-pole parameter smoother per exposed parameter in `EffectProcessor` base, or wrap parameters in a `SmoothedValue<float>` helper.

---

### 5. Floating-Point Phase Drift

`mPhase += phaseInc` accumulates indefinitely over long runtimes, eventually losing precision near large float values.

**Fix:** Wrap with `fmod(mPhase, 2.0f * M_PI)` after each increment. Consider `double` internally where precision matters.

---

### 6. Filter State Not Reset on Coefficient Change

`ParametricEQ` and `SimpleCab` update biquad coefficients without resetting `z1`/`z2` state. This causes zipper noise and potential instability transients on parameter changes.

**Fix:** Reset filter state (or crossfade old→new coefficients) when frequency/Q/gain parameters change beyond a small threshold.

---

### 7. No `GetLatencySamples()` API

`IRCabEffect` and pitch shift effects introduce latency, but `EffectProcessor` has no way to report it. Downstream effects and DAW hosts can't compensate.

**Fix:** Add `virtual int GetLatencySamples() const { return 0; }` to `EffectProcessor`. Override in `IRCabEffect`, `PitchShiftEffect`, and `TransposeEffect`.

---

### 8. Phaser Coefficients Computed Per-Sample

All-pass filter coefficients in the phaser are recalculated every sample rather than at block rate, wasting CPU.

**Fix:** Cache computed coefficients and only recalculate when the LFO phase changes the cutoff frequency by more than a threshold (or recalculate once per block).

---

## Design / Feature Gaps 🟢

| Gap | Details |
|-----|---------|
| No lookahead in dynamics | Compressor/gate detection is purely instantaneous; 1–20 ms lookahead is standard for transparent compression |
| Gain reduction not exposed | UI cannot display compression amount; needs a readable output parameter or callback |
| No sidechain filter | Can't shape the detection signal (e.g., high-pass to reduce low-end pumping in compressors) |
| Mixer per-input delay/pan unused | Parameters declared but executor accumulates all inputs before `Process()` — multi-input interface not implemented |
| No IR file validation | Sample rate / channel count mismatches in cab and reverb IRs not caught at load time |
| No saturation output protection | Distortion/fuzz can produce output > ±1.0; no mandatory limiter in the chain |
| No CPU load estimation | Large convolution IRs can exceed real-time budget with no warning |

---

## Prioritized Action Items

| Priority | Item | Affected Files |
|----------|------|----------------|
| 🔴 Critical | Thread-safe parameter updates | All effect `.h` files, `EffectProcessor.h` |
| 🔴 Critical | `Prepare()` validation guard | `EffectProcessor.h` (base class) |
| 🔴 Critical | Structured error returns from `LoadResource()` | `IRCabEffect`, `NAMEffect`, `IRReverbEffect` |
| 🟡 Important | One-pole parameter smoother | `EffectProcessor.h` + all modulation effects |
| 🟡 Important | `GetLatencySamples()` virtual API | `EffectProcessor.h`, `IRCabEffect`, pitch effects |
| 🟡 Important | Reset biquad state on coefficient change | `ParametricEQEffect`, `SimpleCabEffect` |
| 🟡 Important | Fix phase drift with `fmod` wrap | All modulation effects |
| 🟡 Important | Cache phaser all-pass coefficients | `PhaserEffect` |
| 🟢 Enhancement | Lookahead buffer in compressor/gate | `CompressorVCAEffect`, `GateEffect` |
| 🟢 Enhancement | Sidechain high-pass for compressor detection | `CompressorVCAEffect`, `CompressorOptoEffect` |
| 🟢 Enhancement | Expose gain reduction as readable parameter | `CompressorVCAEffect`, `CompressorOptoEffect`, `LimiterEffect` |
| 🟢 Enhancement | IR file validation on load | `IRCabEffect`, `IRReverbEffect` |

---

## Notes

- All issues above are independent and can be addressed incrementally.
- Thread safety (item 1) should be addressed before any other work ships to production.
- `GetLatencySamples()` is a prerequisite for any future latency compensation feature.
