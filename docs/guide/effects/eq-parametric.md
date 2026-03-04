# Parametric EQ

> Four independent EQ bands that let you precisely shape the frequency balance of your tone.

## What it does

The Parametric EQ gives you detailed control over the tonal balance of your signal. You can boost or cut four specific frequency ranges independently, choosing exactly which frequency to target and how wide or narrow the adjustment is. This lets you fix tone problems (too boomy, too harsh, too muddy) and shape the sound to sit better in a mix or match a particular style.

The EQ includes a **live frequency response curve** — when the EQ node is selected in the Signal Chain Editor, you can see the shape of your EQ in real time as you adjust the controls.

## Bands

The four bands are arranged from low to high frequencies:

| Band | Type | Frequency Range | Gain | Q (Width) |
|------|------|----------------|------|-----------|
| **Low** | Shelf | Fixed low shelf | −12 to +12 dB | — (shelf, no Q) |
| **Low Mid** | Peak (Bell) | Adjustable | −12 to +12 dB | Adjustable |
| **High Mid** | Peak (Bell) | Adjustable | −12 to +12 dB | Adjustable |
| **High** | Shelf | Fixed high shelf | −12 to +12 dB | — (shelf, no Q) |

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Low Gain** | −12 to +12 dB | 0 dB | Boosts or cuts the bass frequencies (shelf). +3 to +6 dB warms up the low end; cutting cleans up mud |
| **Low Freq** | Adjustable | ~100 Hz | Sets the centre/turnover frequency of the low shelf — where the bass adjustment takes effect |
| **Low Mid Gain** | −12 to +12 dB | 0 dB | Boosts or cuts the lower midrange. Cutting here (−3 to −6 dB) reduces boxiness; boosting adds warmth |
| **Low Mid Freq** | Adjustable | ~300 Hz | Sets the centre frequency of the low mid bell — move it to target the exact problematic or pleasant frequency |
| **Low Mid Q** | Adjustable | ~1.0 | Width of the low mid band. High Q = narrow, surgical cut/boost; low Q = broad, gentle shaping |
| **High Mid Gain** | −12 to +12 dB | 0 dB | Boosts or cuts the upper midrange. Boosting adds presence and cut; cutting softens harshness |
| **High Mid Freq** | Adjustable | ~2 kHz | Sets the centre frequency of the high mid bell — the "cut through" frequency for guitar is typically 2–4 kHz |
| **High Mid Q** | Adjustable | ~1.0 | Width of the high mid band |
| **High Gain** | −12 to +12 dB | 0 dB | Boosts or cuts the treble frequencies (shelf). Adding +2 to +4 dB adds air and sparkle; cutting reduces fizz and harshness |
| **High Freq** | Adjustable | ~8 kHz | Sets the turnover frequency of the high shelf |

## Tips

- **Cut first, then boost.** If something sounds wrong, try cutting the offending frequency before boosting elsewhere. Cutting is usually more musical than boosting.
- **Scoop the mids** for a classic 80s heavy rock sound: cut Low Mid at ~400 Hz by −4 to −6 dB to remove boxiness. This sounds exciting solo but can disappear in a band mix — use sparingly for live use.
- **Boost High Mid at 2–4 kHz** (+2 to +4 dB) to help the guitar cut through a dense mix. This is the "presence" range.
- **Cut High at −2 to −4 dB** if the tone sounds harsh or fizzy after a high-gain amp — a small shelf cut here tames the top-end harshness without losing sparkle.
- **Use narrow Q (high value) for surgical fixes** — targeting hum-resonance frequencies or microphone-position artefacts in an IR. **Use wide Q (low value) for overall tonal shaping.**
- Place the EQ **after the cab IR** to shape the final recorded tone; place it **before the amp** to shape what the amp model receives (which changes how it saturates).
