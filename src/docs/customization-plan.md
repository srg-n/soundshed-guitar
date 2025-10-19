# Customization Plan

## Objectives
- Implement an iPlug2-based multi-format plugin exposing NAM DSP with an FX chain and impulse response loader.
- Deliver a webview user interface responsible for browsing, searching, and downloading presets from a remote API while reflecting plugin state.
- Provide a structured preset system with category-aware storage and attachment hashing for NAM and IR assets.

## Workstream Breakdown
1. **Audio Engine**
   - Wrap NeuralAmpModelerCore to support model loading, caching, and block processing.
   - Implement a modular FX chain (gate, drive, EQ, IR convolution) with parameters exposed to iPlug2.
   - Integrate IRManager with convolution loader and file hashing.

2. **Preset System**
   - Define JSON schema for categorized presets, including FX chain state and asset metadata.
   - Implement PresetStorage persistence on disk with hashing via ModelHasher.
   - Integrate network client for search/download, caching responses, and saving to disk.

3. **Webview UI & Bridge**
   - Host SPA resources using iPlug2 webview control.
   - Provide bidirectional messaging to sync parameters, preset lists, and download activity.
   - Implement search UI, preset viewer, and download triggers hitting PresetServiceClient.

4. **Build Outputs & Tooling**
   - Add real VST3/AU/AAX targets with iPlug2 wrappers.
   - Ensure resources are packaged and copied for each format.
   - Document build and dependency setup in README.

## Next Steps
- Flesh out plugin parameter map and UI wiring in `NAMGuitarPlugin`.
- Implement the DSP and preset subsystems per breakdown above.
- Finalize platform-specific entry points and test build configuration.
