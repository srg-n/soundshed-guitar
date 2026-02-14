# Factory Content Bootstrap — Engineering Backlog

## Scope
This backlog implements the plan in `factory-content-bootstrap-plan.md`:
- Factory content pack export
- First-startup install
- Update-time additive apply
- Conflict-safe behavior for user-modified content

## Assumptions
- Stable IDs are used for resources, blends, composites, presets, layouts.
- MVP behavior is skip-on-conflict (no forced overwrite).
- Factory import is non-destructive (no delete of user content).

## Milestones
- **M1:** Schema + dry-run + state tracking
- **M2:** Installer writes resources/blends/composites
- **M3:** Installer writes layouts/presets + dependencies
- **M4:** Export tooling + release wiring
- **M5:** UX polish + diagnostics + hardening

---

## Epic A — Pack Schema & Validation (M1)

### FC-001 Define manifest schema types
**Description**
- Add C++ and TS types for `manifest.json` and item descriptors.

**Acceptance Criteria**
- Parser rejects unsupported `formatVersion`.
- Required fields validated (`packId`, `packVersion`, `items`).
- Unit tests cover valid/invalid manifests.

**Dependencies:** none

### FC-002 Implement hash utilities for pack items
**Description**
- Add deterministic SHA-256 helper for file and JSON payload verification.

**Acceptance Criteria**
- Same input returns same hash on repeated runs.
- Hash mismatch paths return structured error.

**Dependencies:** FC-001

### FC-003 Add `factory-content-state.json` storage
**Description**
- Read/write install state file with schema versioning.

**Acceptance Criteria**
- Missing state file returns default empty state.
- Save is atomic (temp + replace where practical).
- Corrupt file fails safe and logs actionable error.

**Dependencies:** FC-001

### FC-004 Build dry-run diff engine
**Description**
- Compare bundled pack vs local state and emit install/update/no-op/conflict actions.

**Acceptance Criteria**
- Outputs action list grouped by content type.
- No filesystem writes performed.
- Includes conflict reasons in result model.

**Dependencies:** FC-002, FC-003

---

## Epic B — Core Installer (M2–M3)

### FC-010 Create `FactoryContentInstaller` service
**Description**
- New service in core for loading pack, validating, applying, and reporting summary.

**Acceptance Criteria**
- Service can run standalone from a controller entrypoint.
- Returns structured summary `{installed, updated, skipped, conflicts, errors}`.

**Dependencies:** FC-004

### FC-011 Install resources from pack
**Description**
- Extract NAM/IR files to canonical resource content dirs and update resource index/library.

**Acceptance Criteria**
- Imported resources resolve through existing `ResourceLibrary`.
- Existing identical factory resource is no-op.
- Hash mismatch or write failures are reported per-item.

**Dependencies:** FC-010

### FC-012 Install blend library entries
**Description**
- Merge factory blends into blend library with provenance metadata.

**Acceptance Criteria**
- New factory blends become selectable in UI after refresh.
- Existing user-owned blend IDs are never overwritten.
- Conflicts are logged and added to install state.

**Dependencies:** FC-011

### FC-013 Install composite definitions
**Description**
- Write/merge composite definitions to canonical factory-aware store and refresh registry.

**Acceptance Criteria**
- Composite effects appear in catalog post-install.
- Dependencies on missing resources are detected and flagged.

**Dependencies:** FC-011

### FC-014 Install layout images and layouts
**Description**
- Extract layout images and layout content/index files to canonical layout paths.

**Acceptance Criteria**
- Layouts render immediately after `LoadLayoutLibrary()` refresh.
- Layout image references resolve via `layoutLibrary.images`.
- Missing image dependencies produce non-fatal item error.

**Dependencies:** FC-011

### FC-015 Install presets with dependency checks
**Description**
- Install factory presets after resources/blends/composites/layouts.

**Acceptance Criteria**
- Preset list includes installed factory presets.
- Presets with unresolved dependencies are skipped with structured reason.
- User presets are untouched.

**Dependencies:** FC-012, FC-013, FC-014

### FC-016 Persist install state after apply
**Description**
- Update `factory-content-state.json` with applied hashes, version, conflicts.

**Acceptance Criteria**
- Re-running same pack yields no-op.
- Newer pack triggers update actions only where needed.

**Dependencies:** FC-011..FC-015

---

## Epic C — Startup Orchestration (M3)

### FC-020 Add startup trigger for factory install
**Description**
- Call installer during startup sequence (initially post-UI-ready for safer rollout).

**Acceptance Criteria**
- First launch with new pack installs content automatically.
- Startup remains stable if pack missing/corrupt.

**Dependencies:** FC-010, FC-016

### FC-021 Add refresh broadcast after install
**Description**
- Trigger required UI refresh events after successful apply.

**Acceptance Criteria**
- UI reflects new content without app restart.
- Includes state, preset list, composite library, layout library refresh.

**Dependencies:** FC-020

---

## Epic D — Export Tooling (M4)

### FC-030 Add “Export Factory Content Pack” action (UI)
**Description**
- Internal/dev action in Settings to gather selected content and build pack zip.

**Acceptance Criteria**
- Outputs valid pack with `manifest.json` and required folders.
- Uses existing archive/resource request mechanisms where possible.

**Dependencies:** FC-001, FC-002

### FC-031 Include provenance in exported items
**Description**
- Ensure exported JSON embeds factory metadata needed for safe updates.

**Acceptance Criteria**
- Exported items contain source/pack/version/hash fields (or equivalent mapping in manifest/state).

**Dependencies:** FC-030

### FC-032 Add release pipeline hook for bundled pack
**Description**
- CI/package step copies pack to bundled assets path consumed at runtime.

**Acceptance Criteria**
- Installed build includes pack at expected location.
- Missing pack fails CI or emits explicit warning (team choice).

**Dependencies:** FC-030

---

## Epic E — Conflict Policy & UX (M5)

### FC-040 Implement conflict detection rules
**Description**
- Detect local modifications vs last factory hash and apply skip-on-conflict.

**Acceptance Criteria**
- Locally modified factory item is not overwritten.
- Conflict entry added to install state and summary.

**Dependencies:** FC-016

### FC-041 Add install/update summary notification
**Description**
- Present concise result to user/dev with counts and quick diagnostics.

**Acceptance Criteria**
- Summary includes installed/updated/skipped/conflicts/errors.
- Detailed logs available in session log.

**Dependencies:** FC-021, FC-040

### FC-042 Add diagnostics view/command for factory status
**Description**
- UI command/panel to inspect current factory pack + conflicts.

**Acceptance Criteria**
- Shows `lastAppliedPackVersion` and unresolved conflicts.
- Includes re-run dry-run option.

**Dependencies:** FC-004, FC-041

---

## Test Backlog

### FC-T01 First install e2e
- Empty local data + valid pack → all content installed.

### FC-T02 Idempotency
- Same pack applied twice → second run no-op.

### FC-T03 Additive update
- Newer pack with extra items → only new/changed items applied.

### FC-T04 Local modification conflict
- Local edit on factory-tracked item → update skips and records conflict.

### FC-T05 Corrupt pack handling
- Invalid zip/manifest/hash mismatch → safe failure, no partial corruption.

### FC-T06 Dependency enforcement
- Preset referencing missing resource → preset skipped, others continue.

---

## Recommended Implementation Order
1. FC-001 → FC-004
2. FC-010 → FC-016
3. FC-020 → FC-021
4. FC-030 → FC-032
5. FC-040 → FC-042
6. FC-T01..FC-T06

## Definition of Done (Program Level)
- First-time users receive full factory content automatically.
- Upgrades apply new factory content without harming user customizations.
- Exported factory packs are reproducible and validated.
- Conflicts are visible, actionable, and never silently destructive.
