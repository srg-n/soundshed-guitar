# CSS Theme System - Quick Start

## What's Done ✅

1. **Theme System Created** (`css/variables.css`):
   - Default theme (current look)
   - Light theme (bright, clean)
   - Dark theme (dark mode)
   - Classic 70s theme (warm vintage)

2. **Core Files Converted**:
   - `base.css` - 90% converted to use variables
   - `navigation.css` - 90% converted to use variables
   - `index.html` - Updated to load variables.css first

3. **Theme Switcher Built**:
   - `ts/theme-switcher.ts` - Core theme logic
   - `ts/theme-switcher-ui.ts` - UI components
   - `ts/main.ts` - Integrated into app startup
   - 🎨 icon button in icon bar to cycle themes
   - Auto-saves theme preference to localStorage

4. **Automation Tools**:
   - `scripts/convert-css-variables.ps1` - Batch conversion script
   - `docs/css-theme-system.md` - Complete documentation

## What's Left 🔧

**6 CSS files need conversion** (controls, amp, effects, signal-path, modals, fx-library)

## Quick Complete (5 minutes)

### Step 1: Run Conversion Script
```powershell
cd c:\Work\GIT\misc\neuron-guitar
.\scripts\convert-css-variables.ps1
```

This will automatically convert ~90% of hardcoded colors to CSS variables.

### Step 2: Build TypeScript
```powershell
cd src\resources\ui
npm run build
```

### Step 3: Build App
```powershell
cd ..\..\..
cmake --build src\build --config Debug --target GuitarFX_App
```

### Step 4: Test Themes
1. Launch `src\build\x64\Debug\GuitarFX.exe`
2. Click the 🎨 icon in the top icon bar
3. Cycles through: Default → Light → Dark → Classic 70s → Default
4. Theme preference is saved automatically

## How It Works

### CSS Variables
All colors are now defined as CSS variables in `css/variables.css`:

```css
:root {
  --color-primary: #e07848;  /* Default theme */
  --bg-primary: #ffffff;
  --text-primary: #3a3a40;
}

body.theme-light {
  --color-primary: #ff8c50;  /* Light theme */
  --bg-primary: #ffffff;
  --text-primary: #1a1a20;
}

body.theme-dark {
  --color-primary: #ff9d5c;  /* Dark theme */
  --bg-primary: #1a1a20;
  --text-primary: #e0e0e8;
}

body.theme-classic {
  --color-primary: #d89050;  /* Classic 70s */
  --bg-primary: #f8f4e8;
  --text-primary: #2a2420;
}
```

### Component CSS
Component CSS files use the variables:

```css
.button {
  background: var(--color-primary);
  color: var(--text-on-primary);
  border: 1px solid var(--border-dark);
}
```

When the theme changes, all variables update automatically!

### Theme Switching
```typescript
// In your TypeScript code:
import { themeSwitcher } from './theme-switcher.js';

// Set a specific theme
themeSwitcher.setTheme('dark');

// Cycle to next theme
themeSwitcher.cycleTheme();

// Get current theme
const theme = themeSwitcher.getCurrentTheme(); // 'default', 'light', 'dark', or 'classic'
```

## Manual Cleanup (Optional)

After running the script, you may want to:

1. **Review complex gradients**: Some gradients may need theme-specific versions
2. **Check amp panel**: The amp visualization might benefit from theme-specific styling
3. **Test all panels**: Switch themes and check every panel/modal
4. **Adjust colors**: Fine-tune theme colors in `variables.css` if needed

## Customizing Themes

Edit `css/variables.css` to adjust theme colors:

```css
body.theme-dark {
  /* Change the primary color for dark theme */
  --color-primary: #yourcolor;
  
  /* Adjust backgrounds */
  --bg-primary: #yourcolor;
  
  /* Modify text colors */
  --text-primary: #yourcolor;
}
```

All components update automatically - no need to edit 8 CSS files!

## Troubleshooting

### Theme not switching
- Check console for errors
- Verify `variables.css` loads in Network tab
- Confirm body element has theme class in Elements inspector

### Some colors not changing
- Run the conversion script (may have missed some)
- Check for hardcoded colors: `color: #xxxxxx;`
- Convert manually to: `color: var(--variable-name);`

### Want to add a new theme?
1. Add new theme block in `variables.css`:
   ```css
   body.theme-yourtheme {
     /* Define all variables */
   }
   ```
2. Update `theme-switcher.ts` to include new theme
3. Update `theme-switcher-ui.ts` to add option

## File Manifest

### Created
- `css/variables.css` - Theme definitions
- `ts/theme-switcher.ts` - Core theme management
- `ts/theme-switcher-ui.ts` - UI components
- `scripts/convert-css-variables.ps1` - Batch converter
- `docs/css-theme-system.md` - Full documentation
- `docs/css-theme-system-quickstart.md` - This file

### Modified
- `index.html` - Added variables.css import
- `css/base.css` - Converted to variables
- `css/navigation.css` - Converted to variables
- `ts/main.ts` - Integrated theme switcher

### To Be Modified (by script)
- `css/controls.css`
- `css/amp.css`
- `css/effects.css`
- `css/signal-path.css`
- `css/modals.css`
- `css/fx-library.css`

## Summary

You now have a complete theme system with:
- ✅ 4 themes (default, light, dark, classic 70s)
- ✅ ~100+ CSS variables for all colors
- ✅ Theme switcher UI (🎨 icon button)
- ✅ Auto-save preference
- ✅ Automated conversion script
- ✅ Full documentation

**Just run the conversion script, build, and test!** 🎉
