# Signal Level Handling

This document explains the current signal level handling in the simplest possible terms.

## Short Version

Today the app handles level in four main places:

1. A single user-wide input calibration gain can be applied before the chain.
2. Manual gain controls in the preset and effect graph shape level through the chain.
3. NAM amp models can apply automatic output normalization.
4. The mixer can clamp the final output to a configurable ceiling.

There is no hidden global headroom stage that permanently turns everything down.

## The Real Signal Path

For normal playback, level handling happens in this order:

1. Raw stereo input enters the mixer.
2. Raw input diagnostics are measured.
3. Optional mono mode can copy one input channel to both sides.
4. The active user input calibration profile applies one fixed gain change.
5. Legacy mixer-wide auto input could run here, but the controller currently keeps it off.
6. Input diagnostics are measured again after the active input processing.
7. The global pre-chain runs.
   This is where the shared noise gate and transpose live.
8. Each active preset runs its own signal graph.
9. Preset outputs are mixed together with mix and pan.
10. The global post-chain runs.
    This is where shared post effects such as EQ and doubler live.
11. Master gain is applied.
12. Legacy mixer-wide auto output could run here, but the controller currently keeps it off.
13. Output diagnostics are measured.
14. If the final limiter is enabled, the output is clamped to the configured protection ceiling.

## What Actively Changes Level Today

### 1. User Input Calibration

User input calibration is the main global input-level system.

- It is stored in app settings as named profiles.
- The active profile resolves to one fixed gain value in dB.
- That gain is applied once at the mixer input before the pre-chain and before any preset processing.
- If no profile is active, the gain is `0 dB`.
- While the calibration training flow is open, the applied gain is forced to `0 dB` so the capture is honest.

In simple terms: this is the app's main way to compensate for a guitar or interface being too hot or too quiet.

### 2. Manual Gains In The Chain

After the input calibration stage, normal level changes come from explicit controls:

- global input gain and output gain
- per-preset mix and pan in the mixer
- per-effect gain, drive, level, trim, makeup gain, and similar controls
- NAM input gain and output gain parameters

In simple terms: if the sound gets louder or quieter while you edit the rig, it should usually be because of an explicit control.

### 3. NAM Automatic Output Normalization

NAM processing still has an automatic output-level stage.

- The NAM wrappers keep `autoLevelOutput` enabled by default.
- If a product-owned `normalizationGainDb` exists in resource metadata, that value is used first.
- If that metadata is missing but the model exposes loudness metadata, the code aims the model toward the shared nominal operating level.
- The shared nominal target is currently configurable in Settings and defaults to `-18 dBFS`.

In simple terms: NAM output normalization tries to make different amp captures land closer to the same loudness without changing the user's manual controls.

### 4. Final Output Protection

The final protection stage is a hard clamp in the mixer.

- The shared protection ceiling is configurable in Settings and defaults to `-1 dBFS`.
- The final limiter uses that ceiling directly.
- The retired mixer-wide auto output stage also reads the same ceiling, but that path is currently forced off in normal product flow.

In simple terms: this is the last safety stop before audio leaves the mixer.

## What Is Still In The Code But Mostly Retired

Some old level systems still exist in the data model or message flow for compatibility, but they are not the main live behavior anymore.

### Mixer-Wide Auto Input And Auto Output

The mixer still contains simple peak-based auto input and auto output stages, and the UI can still send the old `setAutoLevel` message.

But the controller now forces both of those paths off.

In simple terms: the legacy mixer-wide auto-level system still exists in code, but the product currently treats it as retired.

### NAM Auto Input And Old Calibration Fields

NAM nodes still carry old fields such as:

- `autoLevelInput`
- `calibrationInputLevel`
- `calibrationOutputLevel`

The controller clears this old calibration state when presets load or when NAM level state is reset.

The important part is this: current NAM auto-gain recalculation only applies output-side normalization. It does not currently compute an extra per-model input auto gain.

In simple terms: old NAM input auto-leveling fields still exist for compatibility, but the live behavior is now centered on user input calibration plus NAM output normalization.

### Designed Peak Input

Presets can store a `designedPeakInputDbfs` value.

- The UI can capture the current raw input peak and store it in the preset.
- The value is displayed back to the user.
- It is not part of the live DSP gain path.

In simple terms: this is reference information, not an active gain processor.

## Current Defaults

- User input calibration gain: `0 dB` unless an active profile is selected
- Shared nominal operating level: `-18 dBFS`
- Shared output protection ceiling: `-1 dBFS`
- NAM auto output normalization: on by default
- Mixer-wide auto input: off in current product flow
- Mixer-wide auto output: off in current product flow
- Signal diagnostics: always on

## Settings That Matter Right Now

These settings actively affect signal level behavior:

- `audio.userInputCalibration.profiles`
- `audio.userInputCalibration.activeProfileId`
- `audio.dsp.nominalOperatingLevelDbfs`
- `audio.dsp.outputProtectionCeilingDbfs`

These settings are applied at startup and also when changed at runtime.

In simple terms: if you change the nominal target or protection ceiling in Settings, the running DSP picks it up immediately.

## Diagnostics

The app always streams signal diagnostics.

Three top-level snapshots matter:

- `rawInput`: before mono mode and before user input calibration
- `input`: after the active input processing
- `output`: after master gain and after any retired auto output stage, but before the final hard clamp limiter

The app also records per-node peak, RMS, and clip counts for:

- the global pre-chain
- every active preset graph
- the global post-chain

One important detail: because output diagnostics are measured before the final hard clamp, the diagnostics can show a hotter value than the final delivered sample if the limiter had to catch it.

## The Practical Mental Model

If you want the simplest mental model, use this one:

- Use user input calibration to match your instrument and interface to the app.
- Use normal gain controls to shape tone and balance.
- Let NAM auto output normalization keep different amp captures closer in level.
- Let the final protection ceiling stop the output from going too high.

That is the current system in practice.

## Main Source Files

If you need to inspect the code behind this behavior, start here:

- `core/src/dsp/MultiPresetMixer.cpp`
- `core/src/dsp/MultiPresetMixer.h`
- `core/src/dsp/LevelTargets.h`
- `core/src/dsp/effects/NAMAmpEffect.h`
- `core/src/dsp/effects/OptimizedNAMAmpEffect.h`
- `core/src/dsp/effects/MultiModelNAMAmpEffect.h`
- `core/src/PluginController.cpp`
- `core/src/dispatcher/MessageDispatchSettings.cpp`
- `core/ui/ts/settings.ts`
- `core/ui/ts/views.ts`