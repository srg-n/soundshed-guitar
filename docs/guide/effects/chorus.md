# Chorus

> Thickens the sound by mixing a slightly detuned, time-varying copy of your signal — like a small ensemble playing in unison.

## What it does

Chorus creates one or more copies of your signal, adds a very short delay and a subtle pitch fluctuation using a slow oscillation, then mixes them back with the dry signal. The result is a lush, shimmer-y thickness — notes sound slightly wider, richer, and more three-dimensional. The effect is used everywhere from 80s clean electric tones to 12-string acoustic simulation.

The chorus uses two LFOs (wave generators) running 90° apart so the modulation feels organic and non-repetitive.

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Rate** | 0.1–10 Hz | 1.2 Hz | The speed of the modulation cycle. Lower Rate = slow, dreamy sweep; higher Rate = fast, vibrato-like wobble. Most musical chorus sounds live at 0.5–2 Hz |
| **Depth** | 0–20 ms | 12 ms | How much the delay time fluctuates. More Depth = more pronounced detuning and pitch wobble; less Depth = subtler thickening |
| **Delay** | 1–30 ms | 18 ms | The base delay time of the copied signal. Shorter Delay = tighter, more subtle chorus; longer Delay = more flangy character |
| **Feedback** | 0–95% | 10% | Feeds the output back into the delay line, adding resonance and extra character. Low Feedback keeps the chorus clean; higher Feedback makes it more pronounced and coloured |
| **Mix** | 0–100% | 40% | Blends the chorus (wet) signal with the dry. 40–60% is typical for chorus; higher values increase the effect but can thin the tone |

## Tips

- **Low Rate, medium Depth (Rate ~0.8 Hz, Depth ~10 ms)** produces the classic, subtle chorus heard on clean 80s rhythm guitar — the tone simply sounds thicker and richer.
- **Increase Rate slightly and Depth** for a more animated, three-dimensional texture on arpeggiated parts.
- **Keep Feedback low (0–15%)** for a clean chorus sound. Higher Feedback gives a borderline flanger character — which is fine if you want that edge.
- Chorus sounds best on **clean or lightly overdriven tones**. It can muddy a high-gain signal significantly.
- Place chorus **after the amp and cabinet** in the chain.
