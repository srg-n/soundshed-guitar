# TypeScript Refactor And Cleanup Log - 2026-04-28

## Scope

Reviewed TypeScript source under:

| Area | Files | Lines | Notes |
| --- | ---: | ---: | --- |
| `api/src` | 20 | 5,769 | Cloudflare Worker API routes and libraries |
| `api/test` | 3 | 457 | Vitest coverage for module generation/session flows |
| `core/ui/ts` | 57 | 39,426 | WebView UI, state, messaging, preset/resource flows |
| `core/ui/tests` | 3 | 105 | Vitest coverage for small UI helpers |
| `tools/preset-generator/src` | 8 | 1,393 | Preset/resource pack generation CLI |

Assumptions: this is a refactor and cleanup review, not an implementation pass. Generated output, `dist`, `node_modules`, and dependency code were excluded. Line references are from the current workspace state on 2026-04-28.

## Validation Baseline

| Command | Result | Notes |
| --- | --- | --- |
| `cd api && npm run check` | Failed | `currentParams` narrows to `Record<string, unknown>` where `ModuleNodeContext.currentParams` expects `Record<string, number>` at [api/src/routes/module-sessions.ts](../../api/src/routes/module-sessions.ts#L77). |
| `cd api && npm test` | Passed | 2 files, 4 tests. |
| `cd core/ui && npm run build` | Passed | Includes `postbuild` copy of `jszip.min.js`. |
| `cd core/ui && npm test` | Passed | 3 files, 15 tests. |
| `cd tools/preset-generator && npm run check` | Passed | No generator tests are configured. |

## Implementation Status - 2026-04-28

Implemented in the first cleanup pass:

- Fixed the API `currentParams` type-check failure and added route coverage for numeric-only node context params.
- Extracted API item/pack config parsing to shared helpers and added parser tests.
- Hardened preset-generator resource artifacts so selected downloadable resources must resolve before indexing/writing runs.
- Added preset-generator resource artifact tests and a package-level `npm test` script.
- Switched the UI build to `noEmitOnError: true`.
- Centralized API/UI HTML escaping helpers and escaped the Tone3000 browser error render path.

Current validation after implementation:

- `cd api && npm run check && npm test`: passed.
- `cd core/ui && npm run build && npm test`: passed.
- `cd tools/preset-generator && npm run check && npm test`: passed.

## Priority Log

### P0 - Fix Current TypeScript Failure In API Session Context

`parseNodeContext()` filters `currentParams` values to numbers, but TypeScript still infers the result as `{ [k: string]: unknown }`, which does not satisfy `ModuleNodeContext.currentParams?: Record<string, number>` declared in [api/src/lib/module-generation.ts](../../api/src/lib/module-generation.ts#L12). Evidence is at [api/src/routes/module-sessions.ts](../../api/src/routes/module-sessions.ts#L77).

Suggested cleanup: replace the `Object.fromEntries()` expression with a typed reducer or helper such as `parseNumericRecord(raw): Record<string, number> | undefined`. Add a test that mixed string/number/null params only preserve finite numeric values.

### P0 - Prevent Preset Generator From Shipping Missing Resource Files

`materializeResourceCache()` catches resource download errors and continues after a warning at [tools/preset-generator/src/index.ts](../../tools/preset-generator/src/index.ts#L81). `buildResourceIndex()` then creates an index entry using `sha256Hex(resource.id)` when a resource was not resolved at [tools/preset-generator/src/index.ts](../../tools/preset-generator/src/index.ts#L93), while `writeRunArtifacts()` skips unresolved blobs at [tools/preset-generator/src/index.ts](../../tools/preset-generator/src/index.ts#L137). `validateResourceRefs()` only checks that a resource is present in the index at [tools/preset-generator/src/validate.ts](../../tools/preset-generator/src/validate.ts#L39), so a run can validate even though the indexed content file was never copied.

Suggested cleanup: make selected resource download failures fatal by default, or explicitly remove unresolved resources and any presets that reference them before indexing. Extend validation to assert that every indexed `filePath` exists in the run artifact tree.

### P1 - Make UI Builds Fail Before Emitting Broken Output

The UI compiler config has `noEmitOnError: false` at [core/ui/tsconfig.json](../../core/ui/tsconfig.json#L10). That means a TypeScript failure can still leave generated JS in `dist`, which is risky because the standalone and installer workflows consume built UI artifacts.

Suggested cleanup: set `noEmitOnError` to `true`, then make the build task and CMake UI build path rely on the same failing command. Keep the existing `postbuild` copy behavior, but only after TypeScript succeeds.

### P1 - Centralize HTML Escaping And Safer Rendering Helpers

The UI already exports `escapeHtml()` from [core/ui/ts/utils.ts](../../core/ui/ts/utils.ts#L17), but local copies still exist in [core/ui/ts/blendEditor.ts](../../core/ui/ts/blendEditor.ts#L1541), [core/ui/ts/customEffectDesigner.ts](../../core/ui/ts/customEffectDesigner.ts#L110), [core/ui/ts/layoutRenderer.ts](../../core/ui/ts/layoutRenderer.ts#L551), [core/ui/ts/multiPresetMixer.ts](../../core/ui/ts/multiPresetMixer.ts#L203), [core/ui/ts/resourceBrowser.ts](../../core/ui/ts/resourceBrowser.ts#L1198), [core/ui/ts/settings.ts](../../core/ui/ts/settings.ts#L2567), [core/ui/ts/signalPath.ts](../../core/ui/ts/signalPath.ts#L393), and [core/ui/ts/tone3000Browser.ts](../../core/ui/ts/tone3000Browser.ts#L905). The API also duplicates HTML escaping in [api/src/routes/app.ts](../../api/src/routes/app.ts#L5) and [api/src/lib/email.ts](../../api/src/lib/email.ts#L3).

There is also at least one unsafe render path where an error message is assigned directly into `innerHTML` in [core/ui/ts/tone3000Browser.ts](../../core/ui/ts/tone3000Browser.ts#L321). Many other UI modules use `innerHTML` heavily, usually with escaping but not through one enforceable path.

Suggested cleanup: import the shared UI escaping helper everywhere, add an API-side shared `escapeHtml`, and prefer `textContent` or small DOM builder helpers for dynamic strings. Add an ESLint rule or lightweight grep check to flag unescaped `innerHTML =` assignments for review.

### P1 - Consolidate API Config Parsing And Stop Silent Corruption Hiding

Config JSON parsing is repeated across discovery, item, and pack routes. Discovery parses item/pack config in four places and swallows parse failures with empty catches at [api/src/routes/discovery.ts](../../api/src/routes/discovery.ts#L51), [api/src/routes/discovery.ts](../../api/src/routes/discovery.ts#L68), [api/src/routes/discovery.ts](../../api/src/routes/discovery.ts#L120), and [api/src/routes/discovery.ts](../../api/src/routes/discovery.ts#L146). Similar route-local parsers live in [api/src/routes/items.ts](../../api/src/routes/items.ts#L84) and [api/src/routes/packs.ts](../../api/src/routes/packs.ts#L50).

Suggested cleanup: extract `parseItemConfig()` and `parsePackConfig()` to shared library code with defaulting, schema validation, and structured warnings for malformed DB JSON. Keep route handlers focused on HTTP flow and response shaping.

### P1 - Add Schema Validation At Tool Boundaries

The preset generator casts configuration and seed/cache JSON directly into typed objects. `loadConfig()` returns `readJsonFile<GeneratorConfig>()` at [tools/preset-generator/src/index.ts](../../tools/preset-generator/src/index.ts#L22), `readJsonFile()` parses and casts at [tools/preset-generator/src/fs-utils.ts](../../tools/preset-generator/src/fs-utils.ts#L19), and cache records are parsed without recovery/version checks at [tools/preset-generator/src/cache.ts](../../tools/preset-generator/src/cache.ts#L34) and [tools/preset-generator/src/cache.ts](../../tools/preset-generator/src/cache.ts#L59).

Suggested cleanup: add explicit validators for `GeneratorConfig`, `SeedFile`, API cache records, and resource cache indexes. Validate URL fields, positive TTLs, `minPopularity` range, max preset limits, resource kind enums, and cache schema versions. Invalid config should fail fast with field-path errors; invalid cache should be purged or ignored with a warning.

### P1 - Split Monolithic UI Files Along Existing Boundaries

The largest UI files mix rendering, state mutation, persistence, message handling, and DOM event wiring. Current hotspots are [core/ui/ts/presets.ts](../../core/ui/ts/presets.ts) at 4,135 lines, [core/ui/ts/signalPath.ts](../../core/ui/ts/signalPath.ts) at 4,038 lines, [core/ui/ts/layoutDesigner.ts](../../core/ui/ts/layoutDesigner.ts) at 3,329 lines, [core/ui/ts/toneSharingPanel.ts](../../core/ui/ts/toneSharingPanel.ts) at 3,123 lines, and [core/ui/ts/settings.ts](../../core/ui/ts/settings.ts) at 2,624 lines.

Suggested cleanup: split by behavior rather than by generic utility buckets. Good seams are preset archive import/export, setlist rendering, signal path node rendering, signal path resource collection, layout designer persistence, layout designer sidebar/property editors, tone sharing API client, and settings resource-library management. Keep `state.ts` as the single source of truth and preserve UI message payload compatibility.

### P1 - Reduce API N+1 Query Patterns In Listing Routes

`discoveryRoutes()` loads featured rows and then queries each row's items sequentially at [api/src/routes/discovery.ts](../../api/src/routes/discovery.ts#L22). Item list responses map rows through `toItemResponse()` at [api/src/routes/items.ts](../../api/src/routes/items.ts#L257), and each response loads creator, stats, and user state through separate helpers at [api/src/routes/items.ts](../../api/src/routes/items.ts#L163). Pack list responses similarly call `toPackResponse()` for each row at [api/src/routes/packs.ts](../../api/src/routes/packs.ts#L153), and each pack loads its creator at [api/src/routes/packs.ts](../../api/src/routes/packs.ts#L89).

Suggested cleanup: introduce batch loaders for creators, stats, ratings/favorites, and featured row entries. Keep response builders pure and feed them preloaded maps. This should improve Worker latency and reduce D1 query count under public browsing traffic.

### P1 - Share Tone3000 And Resource Reference Models

Tone3000 and resource-reference shapes are defined in several places: [core/ui/ts/resourceBrowser.ts](../../core/ui/ts/resourceBrowser.ts#L32), [core/ui/ts/tone3000Browser.ts](../../core/ui/ts/tone3000Browser.ts#L37), [core/ui/ts/tone3000.ts](../../core/ui/ts/tone3000.ts#L92), [tools/preset-generator/src/tone3000.ts](../../tools/preset-generator/src/tone3000.ts#L6), and [tools/preset-generator/src/types.ts](../../tools/preset-generator/src/types.ts#L32). The generator even carries a manual sync comment for Tone3000 endpoints at [tools/preset-generator/src/tone3000.ts](../../tools/preset-generator/src/tone3000.ts#L39). Resource refs are richer in UI types at [core/ui/ts/types.ts](../../core/ui/ts/types.ts#L146) than in generator preset nodes at [tools/preset-generator/src/types.ts](../../tools/preset-generator/src/types.ts#L57).

Suggested cleanup: create a small shared TypeScript module for Tone3000 DTOs, endpoint constants, and resource-ref compatibility helpers. If a true shared package is too much, at least centralize UI Tone3000 types and generate/copy tool DTOs from one source.

### P2 - Extract Shared Text Normalization And JSON Parse Helpers In API

`normaliseText()` exists in both [api/src/lib/module-generation.ts](../../api/src/lib/module-generation.ts#L258) and [api/src/routes/module-sessions.ts](../../api/src/routes/module-sessions.ts#L45). `parseJsonValue()` in [api/src/routes/module-sessions.ts](../../api/src/routes/module-sessions.ts#L53) is useful but route-local, while other routes parse JSON ad hoc.

Suggested cleanup: add shared `normaliseText`, `parseJsonObject`, and typed numeric-record helpers under `api/src/lib/validation.ts` or similar. This also gives route tests one place to cover malformed input behavior.

### P2 - Decompose API Module Generation And Session Routes

[api/src/lib/module-generation.ts](../../api/src/lib/module-generation.ts) is 1,807 lines and combines prompt analysis, validation, AI-plan parsing, code generation, WASM byte building, and response hydration. [api/src/routes/module-sessions.ts](../../api/src/routes/module-sessions.ts) mixes route handlers with request parsing and response builders.

Suggested cleanup: split module generation into types, prompt analysis, AI response parsing, code/WASM generation, and hydration. Extract session response mapping from the route file. Keep public response shapes stable, and move existing tests with each extracted unit.

### P2 - Make UI Lifecycle Cleanup Explicit

Several singleton UI controllers bind document/window listeners with anonymous closures, which makes later teardown or hot-reload style reinitialization hard. Examples include generic knobs in [core/ui/ts/controls.ts](../../core/ui/ts/controls.ts#L197), layout designer document handlers in [core/ui/ts/layoutDesigner.ts](../../core/ui/ts/layoutDesigner.ts#L256), dialog key handling in [core/ui/ts/dialogs.ts](../../core/ui/ts/dialogs.ts#L112), and blend editor initialization in [core/ui/ts/blendEditor.ts](../../core/ui/ts/blendEditor.ts#L101).

Suggested cleanup: add a tiny listener registry or `AbortController` pattern for modal/controller classes. Even if controllers remain singletons, explicit lifecycle ownership will reduce duplicate binding risk when future UI panels are rebuilt dynamically.

### P2 - Replace JSON Clone Patterns With A Typed Clone Helper

Deep cloning via `JSON.parse(JSON.stringify(...))` appears throughout UI code, including [core/ui/ts/presets.ts](../../core/ui/ts/presets.ts#L176), [core/ui/ts/state.ts](../../core/ui/ts/state.ts#L289), [core/ui/ts/blendManager.ts](../../core/ui/ts/blendManager.ts#L85), [core/ui/ts/compositeEditor.ts](../../core/ui/ts/compositeEditor.ts#L191), [core/ui/ts/layoutDesigner.ts](../../core/ui/ts/layoutDesigner.ts#L308), and [core/ui/ts/settings.ts](../../core/ui/ts/settings.ts#L1585).

Suggested cleanup: add a project-local `cloneJson<T>()` helper for JSON-shaped preset/layout data, or use `structuredClone()` where runtime support is guaranteed. A named helper makes the data-shape assumption explicit and gives one place to improve behavior later.

### P2 - Improve Preset Generator CLI And Network Resilience

The CLI parses flags manually in [tools/preset-generator/src/index.ts](../../tools/preset-generator/src/index.ts#L26), and the `probe` path parses session JSON directly at [tools/preset-generator/src/index.ts](../../tools/preset-generator/src/index.ts#L368). Resource downloads call `fetch()` without timeout/retry support at [tools/preset-generator/src/cache.ts](../../tools/preset-generator/src/cache.ts#L80), while per-tone model discovery warns and continues at [tools/preset-generator/src/tone3000.ts](../../tools/preset-generator/src/tone3000.ts#L417).

Suggested cleanup: introduce a small CLI parser with `--help`, `--dry-run`, and `--verbose`; add fetch timeout/retry helpers; track skipped resources/model discoveries in `CacheStats` or a run diagnostics section.

### P2 - Expand Test Coverage Around Refactor Targets

Current tests are narrow: API has two module-focused test files, UI has three helper-focused tests, and the preset generator has no test script. The riskiest missing coverage is malformed JSON/config handling, resource download/index consistency, UI escaping/rendering, message normalization, and API list response batching.

Suggested cleanup: add focused tests before large splits. Recommended first suites: API config parsing and `parseNodeContext()`, generator cache/config/resource-index validation, UI `escapeHtml` consolidation, Tone3000 browser error rendering, and signal path resource-ref normalization.

### P3 - Add Linting For Cleanup Rules That Reviews Keep Finding

No ESLint config is present in the workspace. The review repeatedly found patterns that linting can guard cheaply: empty catches, duplicated local helpers, unescaped `innerHTML`, `as any`, and unchecked `JSON.parse()`.

Suggested cleanup: add a minimal ESLint setup per TypeScript package or a shared root config. Start with low-friction rules as warnings, then promote high-signal rules (`no-empty`, `@typescript-eslint/no-explicit-any`, custom restricted syntax for `innerHTML`) after existing code has a cleanup path.

## Suggested Execution Order

1. Fix the API type-check failure and add coverage for numeric `currentParams` narrowing.
2. Fix generator resource artifact validation so pack runs cannot pass with missing resource files.
3. Turn on UI `noEmitOnError` and verify app/VST build paths still fail cleanly.
4. Centralize escaping/render helpers, then fix the raw Tone3000 error render.
5. Extract shared API config parsing and generator schema validation.
6. Split the largest UI/API modules only after targeted tests are in place.