# FX Library

## Key Files
- `src/src/dsp/EffectProcessor.h` — Base interface for all effect processors
- `src/src/dsp/EffectRegistry.h` — Type registration and factory
- `src/src/dsp/effects/` — Individual effect implementations
- `src/src/presets/PresetTypes.h` — `ResourceRef` structure

## Overview

The FX library defines available effect types, their parameters, and resource configuration. Effects register with the `EffectRegistry` for dynamic discovery and instantiation. External resources (NAM models, IRs) are referenced via `ResourceRef` with resolution through the `ResourceLibrary`.

## Effect Registry

### Registration
Effects register at startup via `REGISTER_EFFECT` macro, providing:
- Type ID (string identifier)
- Display name and description
- Category for UI grouping
- Parameter definitions
- Factory function

### Factory
```cpp
// Create effect instance by type ID
EffectProcessor* processor = EffectRegistry::Create("amp_nam");
```

### Queries
- `GetAllTypes()` — List all registered effect types
- `GetTypesByCategory(category)` — Filter by category
- `GetTypeInfo(typeId)` — Get metadata for specific type

## Effect Categories

| Category | Description | Examples |
|----------|-------------|----------|
| `dynamics` | Dynamics processing | Noise gate, compressor, limiter |
| `amp` | Amplifier simulation | NAM amp models |
| `cab` | Cabinet simulation | IR convolution |
| `eq` | Equalization | Parametric EQ, tilt EQ |
| `distortion` | Distortion/saturation | Drive, fuzz, overdrive |
| `modulation` | Modulation effects | Chorus, flanger, phaser |
| `delay` | Time-based delay | Digital, tape, analog |
| `reverb` | Reverberation | Room, hall, plate, spring |
| `utility` | Utility processing | Gain, splitter, mixer |

## Effect Processor Interface

```cpp
class EffectProcessor {
    virtual void Prepare(double sampleRate, int maxBlockSize);
    virtual void Process(float** inputs, float** outputs, int numSamples);
    virtual void Reset();
    virtual void SetParameter(const std::string& id, float value);
    virtual float GetParameter(const std::string& id);
    virtual void SetConfig(const std::string& key, const std::string& value);
    virtual bool LoadResource(const std::string& path);
    virtual int GetLatencySamples();
};
```

## Built-in Effect Types

### NAM Amp (`amp_nam`)
Neural amp model processing.

| Parameter | Range | Default | Unit |
|-----------|-------|---------|------|
| `drive` | 0.0–1.0 | 0.5 | — |
| `tone` | 0.0–1.0 | 0.5 | — |
| `output` | -20..+20 | 0.0 | dB |

**Resource**: NAM model file (`.nam`)

### IR Cabinet (`ir_cab`)
Impulse response convolution for cabinet simulation.

| Parameter | Range | Default | Unit |
|-----------|-------|---------|------|
| `mix` | 0.0–1.0 | 1.0 | — |
| `low_cut` | 20–500 | 80 | Hz |
| `high_cut` | 2000–20000 | 12000 | Hz |

**Resource**: Audio file (`.wav`, `.aiff`)

### Noise Gate (`gate_noise`)
Input noise reduction.

| Parameter | Range | Default | Unit |
|-----------|-------|---------|------|
| `threshold` | -80..0 | -50 | dB |
| `attack` | 0.1–50 | 1.0 | ms |
| `hold` | 0–500 | 50 | ms |
| `release` | 10–1000 | 100 | ms |

### Parametric EQ (`eq_parametric`)
4-band parametric equalizer.

| Parameter | Range | Default | Unit |
|-----------|-------|---------|------|
| `low_freq` | 20–500 | 80 | Hz |
| `low_gain` | -15..+15 | 0.0 | dB |
| `low_q` | 0.1–10 | 0.7 | — |
| `mid1_freq` | 100–2000 | 400 | Hz |
| `mid1_gain` | -15..+15 | 0.0 | dB |
| `mid1_q` | 0.1–10 | 1.0 | — |
| `mid2_freq` | 500–8000 | 2000 | Hz |
| `mid2_gain` | -15..+15 | 0.0 | dB |
| `mid2_q` | 0.1–10 | 1.0 | — |
| `high_freq` | 2000–20000 | 8000 | Hz |
| `high_gain` | -15..+15 | 0.0 | dB |
| `high_q` | 0.1–10 | 0.7 | — |

### Digital Delay (`delay_digital`)
Clean digital delay.

| Parameter | Range | Default | Unit |
|-----------|-------|---------|------|
| `time` | 1–2000 | 300 | ms |
| `feedback` | 0.0–0.95 | 0.3 | — |
| `mix` | 0.0–1.0 | 0.3 | — |
| `high_cut` | 1000–20000 | 8000 | Hz |

### Room Reverb (`reverb_room`)
Room reverberation.

| Parameter | Range | Default | Unit |
|-----------|-------|---------|------|
| `size` | 0.0–1.0 | 0.5 | — |
| `decay` | 0.1–10 | 1.5 | s |
| `damping` | 0.0–1.0 | 0.5 | — |
| `pre_delay` | 0–100 | 10 | ms |
| `mix` | 0.0–1.0 | 0.2 | — |

## Resource References

### ResourceRef Structure
Nodes requiring external files (NAM models, IRs) use `ResourceRef`:

| Field | Type | Description |
|-------|------|-------------|
| `resourceType` | string | Library type: `"nam"` or `"ir"` |
| `resourceId` | string | Library resource ID |
| `filePath` | string | Direct file path (fallback) |
| `embeddedId` | string | Embedded resource reference |

### Resolution Priority
1. **Library reference** — `resourceType` + `resourceId`
2. **Embedded reference** — `embeddedId` (for portable presets)
3. **File path** — `filePath` (user files)

```json
{
  "id": "amp1",
  "type": "amp_nam",
  "resource": {
    "resourceType": "nam",
    "resourceId": "plexi-bright"
  }
}
```

## Resource Library

### Library Structure
```
~/.guitarfx/
└── library/
    ├── index.json           # Catalog with metadata
    ├── nam/
    │   └── models/
    │       └── plexi-bright.nam
    └── ir/
        └── impulses/
            └── 4x12-sm57.wav
```

### LibraryResource Entry
| Field | Type | Description |
|-------|------|-------------|
| `type` | string | `"nam"` or `"ir"` |
| `id` | string | Unique identifier |
| `name` | string | Display name |
| `category` | string | Grouping (e.g., "Marshall", "Fender") |
| `filePath` | string | Actual file location |
| `hash` | string | SHA-256 content hash |
| `size` | int | File size in bytes |

### Content Deduplication
Resources are content-addressed by hash. Duplicate files are detected during import and reference the existing library entry.

### Embedded Resources
For portable preset sharing, resources can be embedded:
- Base64-encoded file content in preset JSON
- Extracted to cache on load
- Hash verification for integrity

## Adding New Effects

1. Implement `EffectProcessor` interface
2. Register with `REGISTER_EFFECT(typeId, DisplayName, category, factory)`
3. Define parameter metadata
4. Place in `src/src/dsp/effects/`
5. Effect appears in UI automatically via registry queries

## See Also
- [Signal Chain](signal-chain.md) — How effects execute in the graph
- [Data Models](data-models.md) — ResourceRef and preset schema
- [User Interface](user-interface.md) — Effect browser UI
