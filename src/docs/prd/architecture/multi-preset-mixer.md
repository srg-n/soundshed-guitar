# Multi-Preset Mixer Architecture

## Goal

Enable multiple active presets to run concurrently from the same input audio, with each preset’s signal chain processed independently and mixed to produce the final output.

## Use Cases

- Layer clean + high-gain chains for tone blending
- Parallel FX chains (e.g., ambient pad + dry rhythm)
- A/B morphing between presets for performance

## High-Level Design

### Components

- `PresetInstance`: Holds `Preset` data and a dedicated `SignalGraphExecutor` prepared with its graph.
- `MultiPresetMixer`: Orchestrates N `PresetInstance`s, fans out input, collects per-preset outputs, applies per-preset mix levels, and sums to final output.
- `ResourceLibrary`: Shared across instances for NAM/IR resolution and caching.

### Data Model

- `ActivePresetSet`:
  - `presets`: array of `{ id, name, executor, mix, mute, solo }`
  - `masterGain`: linear scalar applied post-sum
  - `limitEnabled`: optional simple limiter to prevent clipping

- Parameter addressing: `presetId.nodeId.paramId` to avoid collisions (e.g., `p1_amp_drive`).

### Processing Flow

1. Fan-out input: Each `PresetInstance` receives the same `inputs**` pointers.
2. Per-preset process: Call `executor.Process(inputs, presetOutput, numSamples)` where `presetOutput` is a per-instance buffer.
3. Mix stage: For each sample/channel, sum `presetOutput[ch][i] * mix[preset]` for all active presets (respect `mute/solo`).
4. Master stage: Apply `masterGain`; optionally run a light limiter (lookahead-free) to avoid clipping.

### Semantics

- `mix`: 0.0–1.0 per preset, linear gain applied at mix stage.
- `mute/solo`:
  - `mute`: excludes preset from mix regardless of `mix`.
  - `solo`: if any solo is active, only soloed presets are mixed.
- Trims: Per-preset `global.inputTrim`/`outputTrim` remain effective within each chain; a master gain sits after the sum.

## API Changes

### Plugin Controller

- Add `ActivePresetSet` management:
  - `AddActivePreset(preset)` → returns `presetId`
  - `RemoveActivePreset(presetId)`
  - `SetPresetMix(presetId, value)`
  - `SetPresetMute(presetId, mute)` / `SetPresetSolo(presetId, solo)`
  - `SetMasterGain(value)`

### UI ↔ Plugin Messages

The mixer exposes a simple message schema consumed by the plugin and driven by the WebView UI. All messages are JSON objects sent via the existing bridge.

Outbound (UI → Plugin):

- `addActivePreset`: `{ type: "addActivePreset", presetId: string }`
- `removeActivePreset`: `{ type: "removeActivePreset", presetId: string }`
- `setPresetMix`: `{ type: "setPresetMix", presetId: string, mix: number /* 0..1 */ }`
- `setPresetPan`: `{ type: "setPresetPan", presetId: string, pan: number /* -1..1 */ }` (equal-power law in DSP)
- `setPresetMute`: `{ type: "setPresetMute", presetId: string, mute: boolean }`
- `setPresetSolo`: `{ type: "setPresetSolo", presetId: string, solo: boolean }`
- `setMasterGain`: `{ type: "setMasterGain", gain: number /* linear */ }`
- `setLimiterEnabled`: `{ type: "setLimiterEnabled", enabled: boolean }`

Inbound (Plugin → UI):

- `presetLoaded`: includes `activePresetIds` to seed mixer state:
  `{ type: "presetLoaded", preset: Preset, activePresetIds: string[] }`
- Optional full mixer state sync (if broadcasted):
  `{ type: "mixerState", activePresetIds: string[], presets: Record<string, { mix: number, pan: number, mute: boolean, solo: boolean }>, masterGain: number, limiterEnabled: boolean }`

Message examples:

```json
// Add and tweak a preset
{ "type": "addActivePreset", "presetId": "crystal-clean" }
{ "type": "setPresetMix", "presetId": "crystal-clean", "mix": 0.65 }
{ "type": "setPresetPan", "presetId": "crystal-clean", "pan": -0.25 }
{ "type": "setPresetSolo", "presetId": "crystal-clean", "solo": true }

// Master controls
{ "type": "setMasterGain", "gain": 0.9 }
{ "type": "setLimiterEnabled", "enabled": true }

// Plugin → UI full sync (optional broadcast)
{
  "type": "mixerState",
  "activePresetIds": ["crystal-clean", "lead"] ,
  "presets": {
    "crystal-clean": { "mix": 0.65, "pan": -0.25, "mute": false, "solo": true },
    "lead": { "mix": 0.5, "pan": 0.3, "mute": false, "solo": false }
  },
  "masterGain": 0.9,
  "limiterEnabled": true
}
```

Client-side model (UI):

- `MixerState`:
  - `activePresetIds: string[]`
  - `presets: Record<string, { id: string, mix: number, pan: number, mute: boolean, solo: boolean }>`
  - `masterGain: number`
  - `limiterEnabled: boolean`

Initialization:

- On `presetLoaded`, UI seeds `mixer.activePresetIds` and ensures defaults for any preset id: `mix=1.0`, `pan=0.0`, `mute=false`, `solo=false`.
- If `mixerState` is received, UI merges fields (non-destructive), updating per-preset slots and master controls.

## Threading & Performance

- All preset processing occurs on the audio thread; avoid allocations in `Process`.
- `Prepare(sampleRate, maxBlockSize)` called for each `PresetInstance` on activation.
- Limit max concurrent presets (e.g., 2–4) via config or capability query.
- Resource sharing: `ResourceLibrary` deduplicates models/IRs; processors may share loaded state if safe.

## Validation

- Each preset must have an acyclic graph (`SignalGraphExecutor.IsValid()` true).
- Warn if total mix sum risks clipping; enable `limitEnabled` or auto-set `masterGain`.
- UI provides an optional limiter toggle in the Mixer panel to guard against overs.

## Implementation Steps

1. Create `MultiPresetMixer` with per-instance output buffers and sum logic.
2. Integrate into plugin audio path: replace single `SignalGraphExecutor` call with loop over active presets + mixer.
3. Add controller API and wire WebUI messages.
  - Plugin handles: `addActivePreset`, `removeActivePreset`, `setPresetMix`, `setPresetPan`, `setPresetMute`, `setPresetSolo`, `setMasterGain`, `setLimiterEnabled`.
  - UI renders a Mixer panel with per-preset `mix`/`pan` sliders, `mute`/`solo` toggles, and master controls.
4. Extend parameter addressing to include `presetId`.
5. Update preset loading to create/destroy `PresetInstance`s and prepare executors.
6. Add tests: two-presets sum, mute/solo behavior, clipping guard.

## Future Enhancements

- Crossfade/A-B morph parameter to transition between two presets smoothly.
- Per-preset latency reporting/compensation if effects introduce delay.
- Port-aware split/mix inside each preset, plus multi-channel extensions.
