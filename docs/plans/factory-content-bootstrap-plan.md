# Factory Content Bootstrap Plan

## Assumptions
- Factory content is authored using the existing app editors (presets, resources, blends, composite effects, layouts).
- Factory item IDs are stable across releases.
- Factory updates are additive by default and never silently delete user data.
- If a factory item was modified locally, update should skip/flag conflict rather than overwrite.

## Goals
- Ship first-time users with a complete factory set of:
  - presets
  - resources (NAM/IR)
  - blends
  - composite effects
  - effect layouts (+ layout images)
- Support an internal content-authoring workflow: design in app → export factory pack.
- Apply new factory items automatically on app update.

## Non-Goals (MVP)
- Two-way merge of user edits into updated factory items.
- Cloud sync for factory content.
- Deleting old factory content automatically.

## Current Baseline (Already in Code)
- Startup loads user libraries in `PluginController::Initialize()`.
- Factory presets are read from bundled assets (`presets/factory`) while user presets are separate.
- Composite definitions already load from bundled assets and user storage.
- Existing archive/export paths:
  - Preset archive import/export in UI.
  - Blend archive import/export in UI.
  - Library export in UI.
  - Layout export/import in UI + C++ layout persistence.

This plan reuses those mechanisms and adds one orchestrated factory-pack pipeline.

## Proposed Factory Pack Format

### File name
- `soundshed-factory-content.zip`

### Archive structure
```text
soundshed-factory-content.zip
  manifest.json
  presets/
    <presetId>.json
  resources/
    nam/<resourceId>.<ext>
    ir/<resourceId>.<ext>
    index.json
  blends/
    library.json
  composites/
    <compositeId>.json
  layouts/
    content/<layoutId>.layout.json
    indexes/effect-layouts.json
    images/<imageFile>
```

### `manifest.json` (contract)
```json
{
  "formatVersion": 1,
  "packId": "soundshed-factory",
  "packVersion": "2026.02.14",
  "minimumAppVersion": "0.9.0",
  "createdAt": "2026-02-14T10:00:00Z",
  "items": {
    "presets": [{ "id": "...", "version": 2, "hash": "sha256..." }],
    "resources": [{ "type": "nam", "id": "...", "hash": "sha256...", "file": "resources/nam/..." }],
    "blends": [{ "id": "...", "hash": "sha256..." }],
    "composites": [{ "id": "...", "version": 1, "hash": "sha256..." }],
    "layouts": [{ "layoutId": "...", "lookupKey": "amp_nam::blendA", "hash": "sha256..." }],
    "layoutImages": [{ "imageId": "...", "hash": "sha256...", "file": "layouts/images/..." }]
  }
}
```

## Install State File

Persist factory install state in settings:
- `settings/factory-content-state.json`

```json
{
  "schemaVersion": 1,
  "packId": "soundshed-factory",
  "lastAppliedPackVersion": "2026.02.14",
  "appliedAt": "2026-02-14T10:02:00Z",
  "appliedItems": {
    "preset:clean-jazz": { "hash": "sha256...", "source": "factory" },
    "resource:nam:plexi-bright": { "hash": "sha256...", "source": "factory" },
    "blend:classic-crunch": { "hash": "sha256...", "source": "factory" },
    "composite:vintage-marshall-channel": { "hash": "sha256...", "source": "factory" },
    "layout:amp_nam::defaultLayout": { "hash": "sha256...", "source": "factory" }
  },
  "conflicts": [
    { "key": "preset:user-modified-id", "reason": "local-modified" }
  ]
}
```

## Authoring + Export Workflow

### In-app authoring (existing)
- Use current UI tools to create/edit resources, blends, composites, layouts, presets.

### New export action
- Add `Export Factory Content Pack` action in Settings (internal/dev-only initially).
- Exporter builds `manifest.json` + normalized folders from current selected content.
- Exporter computes SHA-256 hashes for all items/files.

### Release integration
- CI/release copies generated pack into bundled assets path:
  - `<bundled-assets>/factory/content/soundshed-factory-content.zip`

## First Startup + Update Algorithm

### Trigger
- Run once during startup after UI ready (or pre-UI on controller side in a later phase).
- If bundled pack missing, do nothing.

### Decision
1. Read bundled manifest.
2. Read local `factory-content-state.json`.
3. If no local state: treat as first install.
4. If `packVersion` is newer than `lastAppliedPackVersion`: run update.
5. If same/older: no-op.

### Install order (dependency-safe)
1. resources (NAM/IR files + library index entries)
2. blends (may reference resources)
3. composites (may reference resources)
4. layout images
5. layouts
6. presets (may reference all above)

### Conflict policy (MVP)
- If existing item is not factory-tracked: skip (user-owned).
- If existing item is factory-tracked and hash unchanged: overwrite/update allowed.
- If existing item is factory-tracked but hash differs from last factory hash and local changed: skip + add conflict entry.
- Never auto-delete local items absent from new pack.

### Post-apply
- Write updated install state.
- Trigger refresh broadcasts:
  - state
  - preset list
  - composite library
  - layout library

## Runtime Integration Points

### Core (C++)
- Add `FactoryContentInstaller` service (new file pair) responsible for:
  - loading pack from bundled assets
  - manifest validation
  - applying items to canonical storage paths
  - updating `factory-content-state.json`

Suggested integration call sites:
- `PluginController::Initialize()` for pre-UI install (optional phase 2)
- or `OnWebContentLoaded()` for MVP async install + refresh

### UI (TypeScript)
- Add internal command in Settings to:
  - export factory content pack
  - show dry-run/apply status (dev mode)

## Storage Targets (Canonical)
- Presets: settings/presets/user (factory packaged copies can still be tagged metadata `source=factory`)
- Resource binaries: settings/resources/content/<provider>/...
- Resource index: settings/resources/indexes/resources-index.json
- Blends: settings/blends/library.json
- Composites: settings/composites (with factory/user provenance metadata)
- Layouts: settings/layouts/content + settings/layouts/indexes/effect-layouts.json
- Layout images: settings/layouts/content/images

## Provenance Metadata

Every installed factory item should record:
- `source`: `factory`
- `factoryPackId`
- `factoryPackVersion`
- `factoryItemHash`
- `installedAt`

This can live in:
- resource metadata map
- preset/composite/layout JSON optional metadata fields
- and/or centralized `factory-content-state.json` (required)

## Validation Rules
- Manifest `formatVersion` must be supported.
- All required files listed in manifest must exist in zip.
- Item IDs must be non-empty and path-safe.
- Hash mismatch (manifest vs extracted file) aborts that item and logs error.
- Missing dependencies (preset refers to absent resource/blend) => skip preset + report.

## Error Handling
- Fail per-item, not whole-pack, unless manifest invalid/corrupt zip.
- Collect summary:
  - installed count
  - updated count
  - skipped count
  - conflicts count
  - errors
- Emit UI notification + session log summary.

## Rollout Phases

### Phase 1 — Spec + State + Dry Run
- Define manifest schema and parser.
- Implement state file read/write.
- Implement dry-run diff engine (no writes).

### Phase 2 — Apply Engine (Resources + Blends + Composites)
- Implement extraction and writes for resources/blends/composites.
- Provenance recording.

### Phase 3 — Layouts + Layout Images + Presets
- Add layout/layout-image install.
- Add preset install and dependency checks.

### Phase 4 — Export Factory Pack Tooling
- Add in-app pack export action.
- Add release pipeline hook to place pack in bundled assets.

### Phase 5 — Update UX + Diagnostics
- Show install/update summary in UI.
- Expose conflict resolution guidance.

## Test Plan
- First launch with no local data installs all factory content.
- Second launch with same pack performs no-op.
- New pack version installs additive items.
- Existing factory item updated when unchanged locally.
- Locally modified factory item is skipped and reported conflict.
- Corrupt zip/manifest fails safely.
- Missing dependency blocks only dependent item.

## Open Decisions
- Should factory presets live in bundled read-only path only, or be materialized into user storage with provenance?
- Should conflicts create side-by-side items (e.g., `Name (Factory Updated)`) in MVP or later?
- Should pack apply happen before first `state` broadcast (blocking startup) or asynchronously after UI ready?
