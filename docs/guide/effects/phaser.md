# Phaser

> A swirling, cyclical modulation effect that creates peaks and notches in the frequency response — smooth and hypnotic.

## What it does

The Phaser splits your signal in two, runs one copy through a chain of all-pass filters that shift the phase of different frequencies, then blends both copies back together. The interference between the original and phase-shifted signals creates a series of frequency notches that move up and down the spectrum as the oscillation cycles. The result is a smooth, swirling sweep — less jet-like than a flanger, more airy and organic.

The phaser uses 4 stages of all-pass filtering, producing 4 notches in the sweep — a classic configuration.

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Rate** | 0.05–8 Hz | 0.4 Hz | Speed of the phase sweep. Slow Rate = gentle undulation; fast Rate = a noticeable "Leslie cabinet" or vibrato-like wobble. Most musical sounds are between 0.2–2 Hz |
| **Depth** | 0–100% | 80% | How much the sweep covers the frequency range. Higher Depth = a more pronounced, deeper sweep; lower Depth = a subtle shimmer |
| **Feedback** | 0–95% | 30% | Feeds the output of the phaser back into the input, intensifying the notch depth. More Feedback = stronger, more resonant notches with a more "vocal" quality |
| **Mix** | 0–100% | 50% | Blends the phased (wet) signal with the dry. 50% is typical — equal blend. Lower Mix reduces the effect's intensity while keeping some character |

## Tips

- **Slow Rate (0.3–0.5 Hz), Depth ~70%, Feedback ~25%** gives the classic smooth phaser sound beloved in funk, soul, and rock rhythm playing.
- **Higher Rate (1–3 Hz)** starts to feel like a rotating speaker (Leslie) effect, which works well on clean tones.
- **High Feedback** makes the phaser sound more nasal and resonant — useful for more characterful or psychedelic sounds.
- The phaser is one of the most mix-friendly modulation effects — it tends to stay musical at higher Mix levels without muddying a distorted signal as much as chorus or flanger.
- Place after amp and cab; if using phaser and chorus together, try phaser before chorus.
