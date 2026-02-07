# Agent Quickstart

This doc is the minimal, high-signal guide for AI agents working in this repository.

## Minimal Context Bundle

If you only load a few files, use these:

- docs/architecture-overview.md
- docs/signal-chain.md
- docs/fx-library.md
- docs/data-models.md
- docs/user-interface.md
- .github/copilot-instructions.md

## Core Entry Points

- Plugin entry and UI bridge: src/src/GuitarFXPlugin.cpp
- DSP graph executor: src/src/dsp/SignalGraphExecutor.h
- Effect base and registry: src/src/dsp/EffectProcessor.h, src/src/dsp/EffectRegistry.h
- Preset schema and storage: src/src/presets/PresetTypes.h
- UI messages and state: src/resources/ui/ts/messages.ts, src/resources/ui/ts/state.ts

## Common Agent Tasks

### Add a New Effect

1. Implement EffectProcessor in src/src/dsp/effects/.
2. Register it via EffectRegistry in BuiltinEffects.h.
3. Define parameters (ranges, defaults) and category.
4. Update docs/fx-library.md if behavior changes.

### Add or Change a UI Message

1. Update types and handler in src/resources/ui/ts/messages.ts.
2. Update HandleUIMessage in src/src/GuitarFXPlugin.cpp.
3. Keep messages backward compatible and validate payloads.
4. Update docs/user-interface.md for the protocol contract.

### Load a Resource (NAM or IR)

1. GraphNode.resource uses ResourceRef (resourceType + resourceId preferred).
2. Resolve via ResourceLibrary; fall back to embeddedId or filePath.
3. Validate file existence and log errors on failure.
4. Update docs/data-models.md if behavior changes.

## Realtime Safety and Validation

- Audio thread: no allocations, no locks, no blocking I/O.
- Validate parameter ranges and resource presence; fail fast with clear errors.
- Graphs must be acyclic; invalid graphs should not reach Process().

## Build and Test Shortcuts

- Configure: cmake -G "Visual Studio 18 2026" -A x64 -S src -B src/build
- Build App Debug: cmake --build src/build --config Debug --target SoundshedGuitar_App
- UI build: cd src/resources/ui && npm run build
- Tests (Debug): ctest --build-config Debug --output-on-failure
