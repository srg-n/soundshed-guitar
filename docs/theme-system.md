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

GuitarFX uses a CSS variable-based theme system with five built-in themes. Theme preference persists via localStorage.

## Available Themes

| Theme | Class | Description |
|-------|-------|-------------|
| Default | (none) | Medium-contrast, cool grays |
| Light | `.theme-light` | High brightness, clean, modern |
| Dark | `.theme-dark` | Low brightness, reduced eye strain |
| Classic 70s | `.theme-classic` | Warm vintage browns/ambers |
| Worn Pedal | `.theme-gritty` | Gritty, worn metal textures |

## Usage

### Switching Themes (TypeScript)
```typescript
import { themeSwitcher } from './theme-switcher.js';

// Set specific theme
themeSwitcher.setTheme('dark');

// Cycle to next theme
themeSwitcher.cycleTheme();

// Get current theme
const theme = themeSwitcher.getCurrentTheme(); // 'default', 'light', 'dark', 'classic', 'gritty'
```

### UI Control
Click the 🎨 icon in the icon bar to cycle through themes.

## CSS Variables

All colors are defined as CSS variables in `variables.css`:

```css
:root {
  --color-primary: #e07848;
  --bg-primary: #ffffff;
  --text-primary: #3a3a40;
}

body.theme-dark {
  --color-primary: #ff9d5c;
  --bg-primary: #1a1a20;
  --text-primary: #e0e0e8;
}
```

### Variable Categories

| Category | Examples | Usage |
|----------|----------|-------|
| Primary colors | `--color-primary`, `--color-primary-dark` | Brand accent |
| Backgrounds | `--bg-primary`, `--bg-secondary`, `--bg-panel` | Surface colors |
| Text | `--text-primary`, `--text-secondary`, `--text-disabled` | Typography |
| Borders | `--border-dark`, `--border-light` | Separators |
| Controls | `--control-knob-bg`, `--control-toggle-bg-on` | UI controls |
| Overlays | `--overlay-light`, `--overlay-dark` | Transparency layers |
| Shadows | `--shadow-control`, `--shadow-hover` | Elevation |

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
     --color-accent: #yourcolor;
     --bg-primary: #yourcolor;
     --text-primary: #yourcolor;
     /* ...only the tokens this theme overrides; the rest inherit from :root */
   }
   ```

2. Link it in `index.template.html` alongside the other `css/themes/*.css` files.

3. Update `theme-switcher.ts` to include the new theme in the cycle.

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
| Some colors unchanged | Run conversion script or manually convert hardcoded values |
| New component needs theming | Use existing variables; add new ones to all theme blocks |

## See Also
- [User Interface](user-interface.md) — UI architecture
