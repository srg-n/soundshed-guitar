# NAM Blend

> Mixes two or more Neural Amp Models together to create a single hybrid amp tone.

## What it does

The NAM Blend node runs your signal through multiple amp models simultaneously and mixes their outputs. The result is a tone that combines the character of each model — you might blend the low-end weight of one amp with the midrange clarity of another, or merge two different flavours of the same amp to create your own unique capture.

Blends are created and saved in the **Blend Editor** (accessible via the Community tab or resource browser) and appear in your library just like regular single models.

## Parameters

The blend parameters are configured per-model inside the Blend Editor, not on the node itself:

| Control | Range | What it does |
|---------|-------|--------------|
| **Model Mix (per model)** | 0–100% | How much of each model contributes to the output. Values across all models do not need to sum to 100% — they are independent level controls |
| **Snap Mode** | On / Off | When on, mix percentages snap to whole numbers. Useful for creating clean 50/50 or 33/33/33 blends |
| **Interpolate Mode** | On / Off | When on, transitions between mix states are smoothed to avoid clicks when adjusting live |

The node itself uses the same Input Gain and Output Gain controls as a standard [NAM Amp](amp-nam.md) node.

## Tips

- Start with a **50/50 blend** of two different models to hear what the midpoint sounds like, then nudge toward whichever character you want more of.
- Blending two models created from the **same physical amp** at different gain settings is a great way to create an amp that is voiced between two settings.
- Blending models from **different amp families** (e.g. a British and an American) can produce results that work in a mix even if they sound unusual solo — the combination often fills the frequency spectrum in a complementary way.
- Always use a [Cabinet IR](cab-ir.md) after the blend node — it is still an amp head with no cabinet.
