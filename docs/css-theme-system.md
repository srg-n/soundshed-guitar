# CSS Theme System - Implementation Guide

## Overview

GuitarFX now uses a CSS variable-based theme system with 4 themes:
1. **Default** (current medium-contrast theme) - no class
2. **Light** (high brightness, clean) - `.theme-light`
3. **Dark** (low brightness, high contrast) - `.theme-dark`
4. **Classic 70s** (warm vintage browns/ambers) - `.theme-classic`

## Variable Categories

### Primary Colors
- `--color-primary` - Main brand color (orange in default)
- `--color-primary-dark` - Darker variant
- `--color-primary-light` - Lighter variant
- `--text-on-primary` - Text color on primary backgrounds

### Background Colors
- `--bg-primary` - Main background (lightest)
- `--bg-secondary` - Secondary panels
- `--bg-tertiary` - Tertiary panels (darkest in light themes)
- `--bg-quaternary` - Alternative backgrounds
- `--bg-elevated` - Raised/elevated elements
- `--bg-panel` - Panel backgrounds
- `--bg-footer` - Footer bar background

### Text Colors
- `--text-primary` - Main text
- `--text-secondary` - Secondary text
- `--text-tertiary` - Tertiary text (labels, hints)
- `--text-disabled` - Disabled text
- `--text-on-primary` - Text on primary colored backgrounds
- `--text-dark-primary` through `--text-dark-dimmed` - Dark background text variants

### Border Colors
- `--border-darker` - Darkest borders
- `--border-dark` - Dark borders
- `--border-medium` - Medium borders
- `--border-light` - Light borders
- `--border-lighter` - Lightest borders
- `--border-lightest` - Nearly transparent borders

### Control-Specific Colors
- `--control-knob-bg` - Knob background gradient
- `--control-knob-border` - Knob border color
- `--control-knob-pointer` - Knob indicator pointer
- `--control-toggle-bg-off` - Toggle switch off state
- `--control-toggle-bg-on` - Toggle switch on state

### Overlay Colors
- `--overlay-light` - Light overlay (10-20% opacity)
- `--overlay-medium` - Medium overlay (30-40% opacity)
- `--overlay-dark` - Dark overlay (10-20% opacity)
- `--overlay-darker` - Darker overlay (20-30% opacity)
- `--overlay-bright` - Bright overlay (60-80% opacity)
- `--overlay-primary` - Primary color overlay
- `--overlay-accent-light` - Accent color overlay

### Shadow Variables
- `--shadow-control` - Standard control shadow
- `--shadow-hover` - Hover state shadow
- `--shadow-inset` - Inset shadow
- `--shadow-panel` - Panel elevation shadow

### Special Colors
- `--color-accent` - Accent color (green for active states)
- `--color-accent-light` - Light accent variant
- `--color-error` - Error/danger color
- `--color-success` - Success color
- `--color-warning` - Warning color
- `--color-preset-bg` - Preset selector background
- `--color-preset-border` - Preset selector border
- `--color-preset-favorite` - Favorite icon color
- `--color-preset-text` - Preset selector text

## Conversion Status

### ✅ Complete
- [x] `variables.css` - All theme definitions
- [x] `index.html` - CSS imports updated
- [x] `base.css` - ~80% converted
- [x] `navigation.css` - ~90% converted

### 🟡 In Progress
- [ ] `controls.css` - 0% (script ready to run)
- [ ] `amp.css` - 0% (script ready to run)
- [ ] `effects.css` - 0% (script ready to run)
- [ ] `signal-path.css` - 0% (script ready to run)
- [ ] `modals.css` - 0% (script ready to run)
- [ ] `fx-library.css` - 0% (script ready to run)

## Batch Conversion

Run the PowerShell script to auto-convert remaining files:

```powershell
cd c:\Work\GIT\misc\neuron-guitar
.\scripts\convert-css-variables.ps1
```

The script will:
1. Load each CSS file
2. Replace hardcoded colors with CSS variables
3. Replace color gradients with solid variable colors
4. Save updated files
5. Report conversion statistics

## Manual Conversion Patterns

### Pattern 1: Direct Color Replacement
```css
/* Before */
color: #5a5a68;

/* After */
color: var(--text-secondary);
```

### Pattern 2: Gradient to Solid
```css
/* Before */
background: linear-gradient(180deg, #d0d4dc 0%, #b8bcc8 100%);

/* After */
background: var(--bg-secondary);
```

### Pattern 3: RGBA to Overlay Variable
```css
/* Before */
background: rgba(255, 255, 255, 0.3);

/* After */
background: var(--overlay-light);
```

### Pattern 4: Box Shadow (keep structure, extract colors)
```css
/* Before */
box-shadow: 0 2px 4px rgba(0,0,0,0.15);

/* After */
box-shadow: var(--shadow-control);
```

## Testing Themes

1. Build TypeScript:
   ```powershell
   npm run build
   ```

2. Build app:
   ```powershell
   cmake --build build --config Debug --target GuitarFX_App
   ```

3. Run app and click the 🎨 icon to cycle themes

4. Verify all components look correct in each theme:
   - Icon bar and control bar
   - Amp panel and visualization
   - Knobs and controls
   - Signal path nodes
   - Effect panels
   - Modal dialogs
   - FX library

## Theme Palette Reference

### Default Theme (No Class)
- Primary: Orange (#e07848)
- Backgrounds: Cool grays (white → #c8ccd4)
- Text: Dark gray (#3a3a40 → #8a8a98)
- Accents: Green (#48e078)

### Light Theme (.theme-light)
- Primary: Bright orange (#ff8c50)
- Backgrounds: Pure whites/light grays (#ffffff → #f0f2f5)
- Text: Very dark gray (#1a1a20 → #6a6a78)
- High contrast, clean, modern

### Dark Theme (.theme-dark)
- Primary: Vibrant orange (#ff9d5c)
- Backgrounds: Very dark (#0a0a0f → #2a2a30)
- Text: Light gray (#e0e0e8 → #707078)
- High contrast, reduced eye strain

### Classic 70s Theme (.theme-classic)
- Primary: Warm amber (#d89050)
- Backgrounds: Warm browns (#f8f4e8 → #8a7868)
- Text: Dark brown (#2a2420 → #6a5a50)
- Vintage aesthetic, warm tones

## Adding New Components

When creating new UI components:

1. Use existing CSS variables wherever possible
2. If a new color is needed, add it to all 4 theme definitions in `variables.css`
3. Use semantic variable names (e.g., `--color-component-state` not `--blue`)
4. Test component in all 4 themes before committing

## Theme Switcher Integration

The theme switcher is integrated via:
- `ts/theme-switcher.ts` - Core theme management
- `ts/theme-switcher-ui.ts` - UI components
- `ts/main.ts` - Initialization

Users can switch themes via:
- 🎨 icon button in icon bar (cycles through themes)
- localStorage persistence (remembers choice)
- Event system (`themeChanged` event for reactive components)

## Troubleshooting

### Theme not applying
- Check that `variables.css` is loaded first in `index.html`
- Verify body has correct theme class
- Check browser console for CSS errors

### Colors not changing
- Verify CSS variable is defined in all 4 theme blocks
- Check for hardcoded colors that weren't converted
- Use browser DevTools to inspect computed styles

### Gradients look flat
- This is intentional - themes use solid colors for consistency
- Complex gradients (amp panels, etc.) can be theme-specific if needed
- Consider using subtle gradients defined per-theme

## Next Steps

1. **Run conversion script**:
   ```powershell
   .\scripts\convert-css-variables.ps1
   ```

2. **Manual cleanup**: Review converted files for:
   - Complex gradients that need theme-specific versions
   - Hardcoded colors the script missed
   - Components that need special handling

3. **Build and test**:
   ```powershell
   cd src/resources/ui
   npm run build
   cmake --build ../../build --config Debug --target GuitarFX_App
   ```

4. **Theme refinement**: Adjust theme colors based on testing:
   - Update `variables.css` theme definitions
   - No changes needed to component CSS files
   - Rebuild TypeScript and app

5. **Documentation**: Update user documentation with theme switching instructions
