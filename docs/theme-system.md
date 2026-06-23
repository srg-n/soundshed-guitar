# Theme System

## Key Files
- `core/ui/ts/theme-switcher.ts` — Core theme management logic
- `core/ui/ts/theme-switcher-ui.ts` — Theme switcher UI component
- `core/ui/css/variables.css` — Base/default tokens (`:root`) shared by every theme
- `core/ui/css/themes/light.css` — `.theme-light` token overrides
- `core/ui/css/themes/dark.css` — `.theme-dark` token overrides (primary theme)
- `core/ui/css/themes/classic.css` — `.theme-classic` (vintage) token overrides
- `core/ui/css/components.css` — Canonical button/tab/icon-button classes
- `scripts/convert-css-variables.ps1` — Batch conversion script for CSS files

## Overview

GuitarFX uses a CSS variable-based theme system with three active themes (light, dark, vintage/classic). Theme preference persists via backend settings and is reflected in UI state.

## Available Themes

| Theme | Class | Description |
|-------|-------|-------------|
| Light | `.theme-light` | High brightness, clean, modern |
| Dark | `.theme-dark` | Low brightness, reduced eye strain |
| Vintage | `.theme-classic` | Warm vintage browns/ambers |

Legacy persisted values (`default`, `gritty`) are normalized to `dark` in `theme-switcher.ts` for backward compatibility.

## Usage

### Switching Themes (TypeScript)
```typescript
import { themeSwitcher } from './theme-switcher.js';

// Set specific theme
themeSwitcher.setTheme('dark');

// Cycle to next theme
themeSwitcher.cycleTheme();

// Get current theme
const theme = themeSwitcher.getCurrentTheme(); // 'light' | 'dark' | 'classic'
```

### UI Control
Click the 🎨 icon in the icon bar to cycle through themes.

## CSS Variables

Theme tokens are defined in `variables.css` (`:root`) and selectively overridden by theme files:

```css
:root {
  --color-primary: #e07848;
  --bg-primary: #f0f2f5;
  --text-primary: #1c1c24;
}

.theme-dark {
  --color-accent: #ff9d5c;
  --bg-primary: #1a1a20;
  --text-primary: #e0e0e8;
}
```

### Variable Categories

| Category | Examples | Usage |
|----------|----------|-------|
| Accent / semantic | `--color-accent`, `--color-success`, `--status-error-text` | Primary actions and semantic states |
| Backgrounds | `--bg-primary`, `--bg-surface`, `--bg-panel`, `--bg-browser-shell` | App and panel surfaces |
| Text | `--text-primary`, `--text-secondary`, `--text-dark-primary` | Typography and readable contrast |
| Borders | `--border-light`, `--border-medium`, `--border-dark-medium` | Separators and control strokes |
| Controls | `--control-bg`, `--knob-ring-color`, `--toggle-active` | Knobs, toggles, and interactive elements |
| Overlays / shadows | `--overlay-light`, `--overlay-darker`, `--shadow-md` | Depth and transient visual layers |
| Shared component tokens | `--accent-primary`, `--button-bg`, `--button-border-hover` | Canonical button/tab/icon-button styling |

`--color-accent-rgb` is the primitive accent channel token. `--accent-primary-rgb`
is the semantic alias consumed by existing component CSS (for selection outlines,
drag-over states, and overlay mixes). Theme files should override
`--color-accent-rgb` alongside `--color-accent`.

### Hardening Notes

- `variables.css` includes **legacy alias tokens** (for example `--accent`, `--border-color`, `--input-bg`) so older component CSS keeps rendering safely during refactors.
- Dynamic hook tokens used by specific components (for example `--knob-pct`, `--mapped-angle`, `--icon-url`) have default values to avoid invalid-property rendering when runtime state is absent.
- When introducing a new token in component CSS, add a `:root` default first, then override per theme only where needed.

### Component CSS
Components use variables instead of hardcoded colors:

```css
.button {
  background: var(--color-primary);
  color: var(--text-on-primary);
  border: 1px solid var(--border-dark);
}
```

## Canonical UI Components

Core interactive controls have a single source of truth in
`core/ui/css/components.css`. Reuse these classes instead of creating new
per-feature button/tab styles — they are fully theme-driven, so every theme
stays cohesive automatically. A semantic hook class (used by TypeScript) may be
kept alongside the canonical class, e.g. `class="midi-tab-btn tab-btn tab-btn-underline"`.

| Component | Base class | Modifiers |
|-----------|-----------|-----------|
| Button | `.btn` | `.btn-primary`, `.btn-secondary`, `.btn-danger`, `.btn-ghost`, `.btn-small`/`.btn-sm`, `.btn-large`, `.btn-block` |
| Icon button | `.icon-btn` | `.icon-btn-accent`, `.icon-btn-sm`, state: `.active`/`.is-active` |
| Tab | `.tab-btn` | `.tab-btn-underline`, `.tab-btn-vertical`, state: `.active`/`.is-active`/`[aria-selected="true"]` |

Component-specific CSS files should only add layout/positioning tweaks (width,
alignment, placement) on top of these classes. Button hover surfaces are driven
by `--button-bg-hover` / `--button-border-hover` tokens in `variables.css`.

## Adding a New Theme

1. Create `css/themes/yourtheme.css` with the theme's token overrides:
   ```css
   .theme-yourtheme {
     --color-accent: #your-accent;
     --bg-primary: #your-bg;
     --text-primary: #your-text;
     /* ...only the tokens this theme overrides; the rest inherit from :root */
   }
   ```

2. Link it in `index.template.html` alongside the other `css/themes/*.css` files.

3. Update `theme-switcher.ts`:
   - add the theme to `ThemeName` and `THEME_LIST`
   - remove/adjust any relevant legacy mapping in `LEGACY_THEME_MAP`

4. Update `theme-switcher-ui.ts` if adding a menu option.

> Base/default token values live in `css/variables.css` (`:root`). Each theme
> file only needs to override what differs, which keeps per-theme customization
> easy to find and edit.

## Converting Existing CSS

Run the conversion script to replace hardcoded colors with variables:

```powershell
.\scripts\convert-css-variables.ps1
```

### Manual Conversion Patterns

```css
/* Direct color → variable */
color: #5a5a68;          →  color: var(--text-secondary);

/* Gradient → solid variable */
background: linear-gradient(180deg, #d0d4dc 0%, #b8bcc8 100%);
                         →  background: var(--bg-secondary);

/* RGBA → overlay variable */
background: rgba(255, 255, 255, 0.3);
                         →  background: var(--overlay-light);
```

## Building & Testing

```powershell
# Build TypeScript
cd core/ui
npm run build

# Build JUCE standalone app
cmake --build juce/builds --config Debug --target SoundshedGuitar_Standalone

# Test: launch app, click 🎨 icon to cycle themes
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Theme not switching | Check `variables.css` loads first in `index.html` |
| Some colors unchanged | Search for hardcoded values and convert to tokens; if using a legacy alias token, migrate toward canonical token names over time |
| New component needs theming | Use existing variables; add new ones to all theme blocks |

## See Also
- [User Interface](user-interface.md) — UI architecture
