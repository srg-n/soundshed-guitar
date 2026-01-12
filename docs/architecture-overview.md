# Architecture Overview

## Key Files
- `src/src/GuitarFXPlugin.cpp` — Main plugin entry, message handling, state management
- `src/src/GuitarFXPlugin.h` — Plugin interface, parameter definitions
- `src/config/GuitarFXConfig.h` — Branding, plugin metadata, build configuration

## Overview

GuitarFX is a modular, layered audio plugin combining Neural Amp Modeling (NAM) with a flexible signal graph architecture. The system separates concerns across four layers: Platform, Audio Engine, Application, and UI.

## System Layers

```
┌─────────────────────────────────────────┐
│           User Interface Layer          │  WebView SPA (HTML/CSS/TypeScript)
├─────────────────────────────────────────┤
│           Application Layer             │  Preset Manager, Resource Library, Network Client
├─────────────────────────────────────────┤
│           Audio Engine Layer            │  SignalGraphExecutor, Effect Processors, NAM/IR
├─────────────────────────────────────────┤
│           Platform Layer                │  VST3, AU, AAX, Standalone wrappers
└─────────────────────────────────────────┘
```

### Platform Layer
Abstracts plugin format differences and host DAW integration.
- Plugin lifecycle (activation, deactivation)
- Audio buffer routing from host
- Parameter exposure and automation
- State persistence hooks
- **Formats**: VST3 (Windows/macOS), AU (macOS), AAX (Windows/macOS), Standalone

### Audio Engine Layer
Real-time DSP processing with zero allocations in the audio callback.
- **Signal Graph Executor**: Routes audio through effect nodes in topological order
- **NAM DSP Manager**: Loads/runs neural amp models with pre-warming
- **IR Manager**: Partitioned FFT convolution for cabinet simulation
- **Effect Processors**: Registered effect types (gate, EQ, delay, reverb, etc.)

### Application Layer
Coordinates state and business logic.
- **Preset Manager**: CRUD operations, import/export, versioning
- **Resource Library**: NAM/IR catalog with content-addressed deduplication
- **Network Client**: Remote preset search/download (future community sharing)

### User Interface Layer
Web-based SPA in a native WebView container.
- Bidirectional JSON message protocol with the plugin
- Event-based state synchronization
- Platform-specific WebView: WebView2 (Windows), WKWebView (macOS)

## Design Principles

1. **Separation of Concerns** — Each layer has distinct responsibilities
2. **Technology Agnostic** — Core algorithms free of framework dependencies
3. **Performance First** — No allocations in audio thread, lock-free communication
4. **Extensibility** — Plugin architecture for effects, schema-versioned presets

## Threading Model

| Thread | Priority | Purpose |
|--------|----------|---------|
| Audio | Realtime | Audio processing (no blocking) |
| UI | Normal | User interaction, WebView rendering |
| Background | Below Normal | Model loading, network, file I/O |

**Communication:**
- Audio ↔ UI: Lock-free queues, atomic parameter updates
- UI → Background: Task queue
- Background → UI: Completion callbacks

## Performance Targets

| Metric | Target |
|--------|--------|
| Processing Latency | < 10ms @ 44.1kHz, 256 samples |
| CPU Usage (typical) | < 30% single core |
| NAM Model Load | < 2s cold, < 500ms hot swap |
| IR Load | < 200ms |
| Initial Startup | < 3s until UI responsive |
| Memory (typical) | < 300MB |

### Buffer & Sample Rate Support
- **Buffer sizes**: 32–2048 samples (256 recommended)
- **Sample rates**: 44.1, 48, 88.2, 96 kHz (full support); 176.4, 192 kHz (best effort)

## Security Model

### Transport
- HTTPS required for all network communication
- TLS 1.2+ with certificate validation
- Optional certificate pinning for enhanced MITM protection

### Input Validation
- File size and format validation for NAM/IR files
- JSON schema validation for presets with depth limits
- Sanitized search queries and API inputs

### WebView Sandboxing
- No file system access from web context
- Communication only through message bridge
- Content Security Policy restricts script/resource origins

## Error Handling

| Context | Strategy |
|---------|----------|
| Audio path | Graceful bypass, error flag to UI, no interruption |
| Resource loading | Fallback to default/bypass, user notification |
| Network | Cached responses, timeout handling, offline mode |

## See Also
- [Signal Chain](signal-chain.md) — Graph model and execution
- [FX Library](fx-library.md) — Effect definitions and resources
- [User Interface](user-interface.md) — WebView architecture and messaging
- [Data Models](data-models.md) — Preset schema and storage
