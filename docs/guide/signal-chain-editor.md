# Signal Chain Editor

The Signal Chain Editor gives you a visual view of every effect in your current preset and lets you add, remove, reorder, bypass, and configure effects without ever opening a menu.

---

## Opening the editor

Click the **Signal Chain** tab in the navigation bar. The editor shows your current chain as a horizontal row of effect blocks, with your guitar entering from the left and the output on the right.

---

## Adding an effect

1. Open the **FX Browser** panel on the left side of the editor.
2. Effects are grouped by category: Amp, Cabinet, Drive, Dynamics, EQ, Delay, Reverb, Modulation, Pitch, Utility.
3. Click an effect to add it to the end of the chain, or drag it to a specific position between existing nodes.

---

## Removing an effect

Click the **×** button on any effect block. The node is removed and the chain closes the gap automatically.

---

## Reordering effects

Drag an effect block left or right to change its position in the chain. The surrounding blocks shift to make room.

---

## Bypassing an effect

Click the **bypass toggle** (the power button icon) on any block. The block turns grey and the signal passes through it untouched. Click again to re-enable it.

---

## Adjusting parameters

Click on an effect block to expand its parameter controls:

- **Knobs** — click and drag up/down, or double-click to enter a value directly
- **Sliders** — drag left/right; double-click to enter a value
- **Dropdowns** — click to open the option list

Changes take effect immediately as you move the controls — no need to confirm.

---

## Selecting a model or IR resource

Some nodes — the NAM Amp Model and IR Cabinet — need a resource file to work:

1. Click the **resource picker** button (folder icon) on the node.
2. The resource browser opens. You can browse your library or your local files.
3. Click a model or IR to load it into that node.

---

## Creating parallel paths

To split your signal and process it through two separate paths simultaneously:

1. Add a **Splitter** node from the Utility category.
2. The Splitter fans out to two paths. Add effects to each path independently.
3. A **Mixer** node is added automatically at the end of the parallel paths to combine them back.

Use this to blend two different tones, or to keep a dry signal alongside a wet effect path.

---

## EQ visualizer

When a [Parametric EQ](effects/eq-parametric.md) node is selected, a frequency response curve is shown beneath the controls. The curve updates in real time as you move the band controls, giving you a visual guide to the EQ shape.

---

## Saving your chain

Changes to the signal chain are part of the preset. Use **Save Preset** (or **Save As**) in the Preset Browser to preserve your current chain and all parameter values.
