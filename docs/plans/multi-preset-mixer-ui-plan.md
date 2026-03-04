# Multi-Preset Mixer UI — Implementation Plan

## Status: Planning

## Overview

The DSP backend (`MultiPresetMixer`) already supports running multiple presets in parallel with per-preset mix/pan/mute/solo. The bridge layer and mixer state types also exist. What is missing is:

1. A way for the user to **add/remove/reorder presets in the mixer** from the existing preset library UI.
2. A **preset selector in the signal path bar** so the user can choose which active preset's effects chain to view and edit.
3. A **Composite Preset** type that saves the entire multi-preset mixer configuration (which presets, at what mix/pan) for later recall — selectable from the preset library just like regular presets.

---

## Concepts

### Active Presets (Mixer Slots)
The set of presets currently loaded and running in the `MultiPresetMixer`. Each slot has:
- A stable `slotId` (e.g. `"p1"`, `"p2"`) used to route DSP controls
- A reference to a regular `Preset` by ID
- Per-slot mix, pan, mute, solo

### Signal Path Focus
A **UI-only** concept: which active preset's signal chain is currently displayed in the signal path bar and editable. Does not affect DSP routing. Works like a tab selection.

### Composite Preset
A saved snapshot of a multi-preset mixer configuration. Contains:
- An ordered list of preset-slot references (preset IDs + mix/pan/mute/solo)
- Master gain and limiter setting
- Metadata (name, description, tags, timestamps)

Composite presets are their own top-level entity (separate from `Preset`). They appear in a dedicated section in the preset library popover.

---

## Data Models

### New: `CompositePresetSlot`
```typescript
interface CompositePresetSlot {
  slotId: string;      // stable DSP instance ID, e.g. "p1"
  presetId: string;    // references a regular Preset by ID
  mix: number;         // 0..1 linear gain
  pan: number;         // -1..1 (L..R)
  mute: boolean;
  solo: boolean;
}
```

### New: `CompositePreset`
```typescript
interface CompositePreset {
  id: string;
  name: string;
  description?: string;
  tags?: string[];
  createdAt?: string;
  modifiedAt?: string;
  slots: CompositePresetSlot[];
  masterGain: number;
  limiterEnabled: boolean;
}
```

### New state fields
```typescript
// in UiState
focusedMixerPresetId: string | null;  // which slot's chain is shown in signal path
compositePresets: CompositePreset[];   // loaded composite preset library
```

### Changes to `MixerPresetState`
Add the `slotId` field and the preset name for display:
```typescript
interface MixerPresetState {
  id: string;       // preset ID
  slotId: string;   // stable DSP instance ID
  mix: number;
  pan: number;
  mute: boolean;
  solo: boolean;
}
```

---

## Affected Files

### TypeScript UI (`core/ui/ts/`)

| File | Change |
|------|--------|
| `types.ts` | Add `CompositePreset`, `CompositePresetSlot`; add `focusedMixerPresetId` and `compositePresets` to `UiState`; add `slotId` to `MixerPresetState` |
| `state.ts` | Add `focusedMixerPresetId: null` and `compositePresets: []` to initial `uiState`; add `getFocusedMixerPreset()` helper |
| `signalPath.ts` | `renderSignalPathBar()` reads `focusedMixerPresetId` when mixer has multiple slots; render a **preset selector tab row** above the graph when multiple slots active |
| `views.ts` | Update `buildMixerMarkup()`: add Remove (×) button per row, "View Chain" focus button per row, highlight focused row; add "Save Multi-Rig" button at top |
| `presets.ts` | Add "Add to Mixer +" action in preset list items (via `applyPresetFromLibrary` or separate button); add "Multi-Rig" section in preset library popover to list composite presets; render/load composite presets |
| `messages.ts` | Handle `compositePresetList`, `compositePresetLoaded`, `compositePresetSaved` messages |
| `bridge.ts` | Add `saveCompositePreset(name, description?)`, `loadCompositePreset(id)`, `getCompositePresetList()`, `removeCompositePreset(id)` |
| **New: `multiPresetMixer.ts`** | Composite preset UI: render composite preset list chips, render composite preset detail, handle load/save/delete |

### HTML (`core/ui/index.html`)

| Location | Change |
|----------|--------|
| Preset library popover `.preset-library-body` | Add a "Multi-Rig" tab / section alongside existing preset list |
| Signal path bar | No HTML change needed; tab row is rendered dynamically via JS |

### C++ Core (`core/src/`)

| File | Change |
|------|--------|
| **New: `presets/CompositePresetTypes.h`** | `CompositePreset`, `CompositePresetSlot` structs + JSON serialization |
| **New: `presets/CompositePresetStorage.h/.cpp`** | Save/load composite presets to disk (same pattern as `PresetStorage`) |
| `PluginController.cpp` | Handle new messages: `saveCompositePreset`, `loadCompositePreset`, `getCompositePresetList`, `removeCompositePreset` |
| `MessageDispatcher.cpp` | Route new message types |

---

## Phase 1 — Signal Path Preset Selector (UI-only)

**Goal**: When multiple presets are active, the user can visually select which one's signal chain to view in the signal path bar.

**Scope**: Purely frontend. No new backend messages needed.

### Changes

#### `types.ts`
Add `focusedMixerPresetId: string | null` to `UiState`.

#### `state.ts`
- Add `focusedMixerPresetId: null` to initial state.
- Add helper `getFocusedMixerPreset(): Preset | null` — when multiple mixer slots are active, return the preset cache entry for `focusedMixerPresetId`; fall back to `activePresetId`.
- When a preset is added/removed from the mixer, auto-select the first slot as focused if focused slot is no longer active.

#### `signalPath.ts`
- `renderSignalPathBar()`: when `mixer.activePresetIds.length > 1`, render a `<div class="signal-path-preset-tabs">` above the nodes with one tab per active preset. Clicking a tab sets `uiState.focusedMixerPresetId` and re-renders.
- Use `focusedMixerPresetId ?? activePresetId` to pick which preset's graph to display.
- Tab shows preset name + muted/solo indicator.
- While in single-preset mode (1 slot), behavior is identical to current (no tabs visible).

#### `views.ts` (mixer)
- Add a "focus" icon button to each mixer row that sets `focusedMixerPresetId`.
- Highlight the row whose `slotId` matches `focusedMixerPresetId`.

**Verification**: Build TypeScript, launch app with 2+ active presets, confirm tab switching changes the signal chain displayed without affecting audio.

---

## Phase 2 — Add to Mixer UX

**Goal**: Make it easy to add presets to the mixer from the preset library.

### Changes

#### Preset List Items
In `renderPresetList()` (in `presets.ts`/`views.ts`), add an "Add to Mixer" (`+`) icon button to each preset list item.  
- Clicking it calls `addActivePreset(presetId)` (already in `bridge.ts`).
- If the preset is already in the mixer, show a highlighted/filled icon instead (visual indicator drawn from `uiState.mixer.activePresetIds`).

#### Preset Details Panel
Add a "Add to Mixer" / "Remove from Mixer" toggle button in the preset details action row.  
Currently the `presetLoaded` flow only replaces the single active preset. In multi-preset mode, loading from the library via "Add to Mixer" should call `addActivePreset` instead of `loadPreset`.

#### Mixer Panel
- Add a Remove (×) button to each `mixer-row`.
- Clicking it calls `removeActivePreset(slotId)` and refreshes.

**Verification**: Build TypeScript, confirm presets can be added/removed via the library and mixer panel.

---

## Phase 3 — Composite Presets

**Goal**: Save and recall the full multi-preset mixer configuration as a named composite preset.

### 3a — TypeScript Types + State

Add to `types.ts`:
```typescript
interface CompositePresetSlot {
  slotId: string;
  presetId: string;
  mix: number;
  pan: number;
  mute: boolean;
  solo: boolean;
}

interface CompositePreset {
  id: string;
  name: string;
  description?: string;
  tags?: string[];
  createdAt?: string;
  modifiedAt?: string;
  slots: CompositePresetSlot[];
  masterGain: number;
  limiterEnabled: boolean;
}
```

Add to `UiState`:
```typescript
compositePresets: CompositePreset[];
```

### 3b — Bridge Messages

New outbound messages (UI → Engine):
| Type | Payload | Description |
|------|---------|-------------|
| `saveCompositePreset` | `{name, description?}` | Save current mixer state as composite preset |
| `loadCompositePreset` | `{id}` | Load a composite (sets all mixer slots) |
| `getCompositePresetList` | `{}` | Request list of saved composite presets |
| `removeCompositePreset` | `{id}` | Delete a composite preset |

New inbound messages (Engine → UI):
| Type | Payload | Description |
|------|---------|-------------|
| `compositePresetList` | `{compositePresets: CompositePreset[]}` | Full list of saved composite presets |
| `compositePresetSaved` | `{compositePreset: CompositePreset}` | Confirmation after save |
| `compositePresetLoaded` | `{compositePreset: CompositePreset, activePresetIds: string[]}` | Confirmation after load |

### 3c — UI Components (`multiPresetMixer.ts` — new file)

Functions:
- `renderCompositePresetList()`: renders the List of composite presets in the library panel
- `renderCompositePresetChip(cp: CompositePreset)`: single item card (name, slot count, load/delete buttons)
- `handleCompositePresetList(composites: CompositePreset[])`: handles `compositePresetList` message, updates state and re-renders
- `handleSaveCompositePreset()`: opens a save modal (name + description), then calls `bridge.saveCompositePreset()`
- `handleLoadCompositePreset(id: string)`: calls `bridge.loadCompositePreset(id)`, shows a loading notification

### 3d — Preset Library UI (`index.html` + `presets.ts`)

In the preset library popover, add a **"Multi-Rig"** tab header alongside existing preset list:
```
[Presets]  [Multi-Rig]
```

When "Multi-Rig" is selected:
- Show `renderCompositePresetList()` content
- Show a "Save Current as Multi-Rig..." button at the top (only enabled when 2+ presets active)

### 3e — C++ Backend

#### `CompositePresetTypes.h` (new)
```cpp
namespace guitarfx {
  struct CompositePresetSlot {
    std::string slotId;
    std::string presetId;
    double mix = 1.0;
    double pan = 0.0;
    bool mute = false;
    bool solo = false;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CompositePresetSlot, slotId, presetId, mix, pan, mute, solo)
  };

  struct CompositePreset {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> tags;
    std::string createdAt;
    std::string modifiedAt;
    std::vector<CompositePresetSlot> slots;
    double masterGain = 1.0;
    bool limiterEnabled = false;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CompositePreset, id, name, description, tags, createdAt, modifiedAt, slots, masterGain, limiterEnabled)
  };
}
```

#### `CompositePresetStorage.h/.cpp` (new)
- `SaveCompositePreset(const CompositePreset& cp, const std::filesystem::path& dir)` — writes `<id>.composite.json`
- `LoadCompositePreset(const std::string& id, const std::filesystem::path& dir)` — reads file
- `ListCompositePresets(const std::filesystem::path& dir)` — scans for `*.composite.json` files
- `DeleteCompositePreset(const std::string& id, const std::filesystem::path& dir)` — removes file

Storage location: same user data directory as regular presets, subdirectory `composite-presets/`.

#### `PluginController.cpp`
Handle new message types:

**`saveCompositePreset`**: 
1. Build `CompositePreset` from current `mMixer` state (iterate `GetActivePresetIds()`, get `GetPresetConfig(id)`)
2. Assign UUID, set timestamps
3. Call `CompositePresetStorage::SaveCompositePreset()`
4. Send `compositePresetSaved` response

**`loadCompositePreset`**:
1. Load `CompositePreset` from storage
2. Clear all current mixer instances (`RemoveActivePreset` for each)
3. For each slot: load the referenced preset from library/storage, call `AddActivePreset()`
4. Apply mix/pan/mute/solo settings
5. Send `compositePresetLoaded` response with `activePresetIds`

**`getCompositePresetList`**:
1. Scan composite presets directory
2. Serialize list
3. Send `compositePresetList` response

**`removeCompositePreset`**:
1. Validate ID (reject path traversal)
2. Call `CompositePresetStorage::DeleteCompositePreset()`
3. Send updated `compositePresetList`

---

## Implementation Sequence

```
Phase 1: Signal path preset selector (UI-only, self-contained)
  1. types.ts — focusedMixerPresetId in UiState
  2. state.ts — initial value + getFocusedMixerPreset() helper + auto-select logic
  3. signalPath.ts — tab row rendering, tab click handler, preset resolution
  4. views.ts — mixer row focus button + highlight
  → Build + test

Phase 2: Add/Remove from library (UI-only bridge calls already exist)
  5. views.ts / presets.ts — Add to Mixer + badge in list
  6. views.ts — Remove button in mixer row
  → Build + test

Phase 3a: Composite preset types + state (no build yet)
  7. types.ts — CompositePreset, CompositePresetSlot
  8. state.ts — compositePresets: []
  9. bridge.ts — new outbound message functions

Phase 3b: C++ composite preset backend
  10. CompositePresetTypes.h (new)
  11. CompositePresetStorage.h/.cpp (new)
  12. PluginController.cpp — new message handlers
  13. CMakeLists.txt — add new .cpp files
  → Build C++ (Debug App)

Phase 3c: UI composite preset components
  14. multiPresetMixer.ts (new) — list rendering + handlers
  15. messages.ts — handle compositePresetList, compositePresetSaved, compositePresetLoaded
  16. index.html — Multi-Rig tab in preset library popover
  17. presets.ts — bind Multi-Rig tab + Save Multi-Rig button
  → Build TypeScript + full build test

Phase 3d: Docs + backwards compatibility check
  18. docs/data-models.md — add CompositePreset section
  19. docs/user-interface.md — add new message types table entries
  → Verify all existing tests pass
```

---

## UX Wireframes (text)

### Signal Path Bar — Multiple Presets Active
```
┌─────────────────────────────────────────────────────────────────────┐
│  [Plexi Crunch ●] [Mesa Clean] [+ Add]                             │  ← preset tabs
│──────────────────────────────────────────────────────────────────────│
│  ○ Input → [Gate] → [NAM: Plexi] → [IR: 4x12] → [Reverb] → ○ Out  │  ← Plexi Crunch chain
└─────────────────────────────────────────────────────────────────────┘
  ● = currently focused/editing preset
```

### Mixer Panel — Multiple Presets Active
```
┌─────────────────────────────────────────────────────────────────────┐
│  Mixer                                          [Save Multi-Rig…]   │
│──────────────────────────────────────────────────────────────────────│
│  ● Plexi Crunch      [Solo] [Mute] [View Chain ▶] [× Remove]       │
│    Mix ──────●────  Pan ──●──                                        │
│──────────────────────────────────────────────────────────────────────│
│    Mesa Clean        [Solo] [Mute] [View Chain ▶] [× Remove]        │
│    Mix ───●────────  Pan ──●──                                       │
│──────────────────────────────────────────────────────────────────────│
│  Master Gain ──────────●──  [✓ Limiter]                              │
└─────────────────────────────────────────────────────────────────────┘
```

### Preset Library — Multi-Rig Tab
```
┌─────────────────────────────────────────────────────────────────────┐
│  [Presets]  [Multi-Rig]                                              │
│────────────────────────────────────────────────────────────────────  │
│  [⊕ Save Current as Multi-Rig…]  (enabled when 2+ presets active)   │
│                                                                       │
│  ┌─────────────────────────────────────────────────────┐            │
│  │  Lead + Reverb Stack                  2 presets  [Load] [×]      │
│  │  Plexi Crunch  ▸  Mesa Clean                                      │
│  └─────────────────────────────────────────────────────┘            │
│  ┌─────────────────────────────────────────────────────┐            │
│  │  Dual Amp Clean                       2 presets  [Load] [×]      │
│  │  Fender Twin  ▸  AC30 Clean                                       │
│  └─────────────────────────────────────────────────────┘            │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Backward Compatibility Notes

- Single-preset mode (1 active preset): all existing behavior unchanged. Tabs only appear with 2+ slots.
- `activePresetId` in `uiState` continues to represent the "primary" preset (first mixer slot or the one selected via single preset load). The `focusedMixerPresetId` is additional UI-layer state.
- Composite presets use a separate storage directory and message types. They do not conflict with regular presets.
- `loadCompositePreset` will clear all current mixer instances — show a confirmation dialog before replacing if the current mixer state is unsaved.
- `slotId` assignment on the C++ side: generate deterministic IDs (`"p" + index`) or use UUIDs; the UI uses presetId (not slotId) as the key in `mixer.presets` map for simplicity. Revisit if parallel same-preset-different-slot use cases arise.

---

## Open Questions

1. **Should loading a composite preset also update `activePresetId`?** Suggested: yes, set it to the first slot's presetId so signal chain display auto-focuses on slot 1.
2. **Should composite presets be importable/exportable in the same .presetz archive format?** Suggested: defer to a follow-up; for now store locally only.
3. **Maximum mixer slot count?** DSP supports N; recommend a soft UI cap of 4 for now to keep the tab row manageable.
4. **Embedded resources in composite presets?** Composite presets reference regular presets by ID — embedded resource handling deferred. The referenced presets must exist locally.
5. **Conflict on loadCompositePreset when a referenced preset is missing?** Show an error notification listing the missing preset names and abort the load rather than silently skipping slots.
