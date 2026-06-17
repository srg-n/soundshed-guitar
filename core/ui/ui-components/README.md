# UI Components (Alpine.js)

This directory contains the split HTML fragments for the Soundshed Guitar UI.

## Usage
- Fragments are inlined at build time by `scripts/assemble-html.js` using `<!--#include:ui-components/xxx.html-->` markers in `index.html` (or `index.template.html`).
- Run `npm run build` (or `npm run build:html`) after edits.
- CMake tracks these files for rebuilds.

## Alpine.js Integration
- Global stores live in `ts/alpine.ts` (e.g. `$store.ui`, `$store.fxSelector`).
- Use directives: `x-show`, `x-for`, `:class`, `x-on:click`, `x-model`, `x-text`, etc.
- Component data providers and logic stay in TypeScript.
- Call `Alpine.store('xxx').update...()` or use data factories from TS modules.
- For dynamic inserted content: `initAlpineTree(el)`.

## Adding a new component
1. Create `some-component.html` here with the markup + Alpine directives.
2. Add `<!--#include:ui-components/some-component.html-->` in the shell.
3. (Optional) Add store slice + push/update functions in `ts/alpine.ts` or feature module.
4. Migrate related `render*` / `innerHTML` code in the matching `.ts` file to update the store instead.
5. Rebuild and test.

## Current extractions (full port in progress - index.template.html is the thin editable source with markers; run build to produce runtime index.html)
Top-level:
- splash-screen.html
- header-icon-bar.html (Alpine nav tabs + store)
- control-bar.html (thin shell)
  - input-control-group.html
  - preset-group.html (further includes below)
  - output-control-group.html
  - jam-player-dock.html
- jam-floating-player-root.html
- preset-toolbar-row.html
- preset-selector-row.html
- preset-library-popover.html (Alpine tag chips)
- signal-path-bar.html
- fx-selector-panel.html
- main-content.html (thin)
  - panels/advanced-panel.html
  - panels/jam-panel.html
  - panels/sharing-panel.html
- footer-bar.html
- notification-area.html

Modals (all extracted to modals/):
- custom-effect-designer-modal.html
- riff-save-modal.html
- riff-capture-modal.html
- tone3000-details-modal.html
- metronome-modal.html
- user-input-calibration-modal.html
- tuner-modal.html
- eq-modal.html
- blend-editor-modal.html
- resource-browser-modal.html
- blend-model-browser-modal.html
- save-preset-modal.html
- tone-sharing-pack-view-modal.html
- tone-sharing-publish-modal.html
- tone-sharing-consent-modal.html
- tone-sharing-signin-modal.html
- tone-sharing-pack-modal.html
- layout-designer-modal.html
- dialog-modal.html
- tone3000-required-modal.html
(and any additional discovered)

See plan.md for phases and goals. All logic remains in TypeScript. Edit index.template.html + fragments.
