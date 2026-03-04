# Simple Cabinet

> A lightweight cabinet simulator with basic tone shaping controls — no IR file needed.

## What it does

The Simple Cabinet uses a set of tone-shaping filters to mimic the broad character of a guitar speaker cabinet — rolling off very low frequencies (the rumble that cabinets naturally don't reproduce), shaping the mid-frequency peak that gives cabinets their presence, and taming the harsh highs above the speaker's range. It is less detailed than an [IR Cabinet](cab-ir.md) but costs very little CPU and requires no IR file.

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Bass** | 0–100% | 0% | Controls the low-frequency shelf. Increase to add warmth and body; reduce (left) to thin out the low end |
| **Presence** | 0–100% | 0% | Boosts or cuts the upper midrange (around 2–3.5 kHz) — the frequency range that gives the guitar presence and cut in a mix |
| **Brightness** | 0–100% | 0% | Controls the high-frequency rolloff. Increase to let more treble through; decrease for a darker, warmer cabinet sound |
| **Mix** | 0–100% | 0% | Blends between the dry amp signal and the cabinet-processed signal |

> **Note:** All controls default to their centre/off position. The Simple Cabinet has no effect until you raise the Mix control.

## Tips

- **Raise Mix to 100%** first to hear the cabinet character, then adjust Bass, Presence, and Brightness to taste.
- As a starting point for a classic voiced tone: Bass ~40%, Presence ~50%, Brightness ~30%, Mix 100%.
- Use **Simple Cabinet** when you want a quick cabinet sound without loading an IR — useful for sketching tones quickly or when CPU is limited.
- For highest quality recordings, prefer [IR Cabinet](cab-ir.md) — it will sound more like a real cabinet in a real room.
- The Presence control is the most powerful one for cut in a mix — a small boost (30–50%) makes the guitar sit forward without sounding harsh.
