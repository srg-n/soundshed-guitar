# VCA Compressor

> A fast, punchy compressor that tightens your attack, controls dynamics, and increases sustain.

## What it does

A compressor reduces the difference between the loudest and quietest parts of your playing. The VCA (Voltage Controlled Amplifier) style compressor reacts quickly and precisely — it is well-suited to tightening up pick attack, adding punch to rhythm playing, and squeezing more sustain out of lead lines.

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Threshold** | −60 to 0 dB | ~−20 dB | The level above which the compressor starts reducing gain. Lower the threshold to compress more of your signal |
| **Ratio** | 1:1 to 20:1 | ~4:1 | How aggressively the compressor reduces the signal above the threshold. 2:1 is gentle; 10:1+ is heavy squash; 20:1 is near-limiting |
| **Attack** | 0.1–500 ms | ~10 ms | How quickly the compressor clamps down after the signal crosses the threshold. A slower attack (10–30 ms) lets the initial pick transient through, adding perceived punch |
| **Release** | 10–2000 ms | ~200 ms | How quickly the compressor lets go after the signal drops below the threshold. Shorter release pumps more; longer release is smoother and more transparent |
| **Knee** | 0–24 dB | ~6 dB | Controls the transition around the threshold. 0 dB = hard knee (abrupt compression onset); higher values = soft knee (gradual onset, more transparent) |
| **Makeup Gain** | 0–24 dB | 0 dB | Adds back the overall volume lost through compression. Raise this until the compressed signal matches the bypassed level |
| **Mix** | 0–100% | 100% | Blends the compressed signal with the uncompressed dry signal (parallel compression). 100% = fully compressed; lower values retain some of the dry dynamic punch |

## Tips

- **Match levels with makeup gain** — turn the compressor on and off and adjust Makeup Gain until the volumes sound equal. This reveals what the compressor is really doing to the tone rather than the loudness.
- **Slow attack + high ratio** is good for country-style "squish" — the pick transient jumps through before the compressor grabs it, creating a percussive click followed by a compressed sustain.
- **Fast attack + low ratio** is good for consistent rhythm playing — levelled strumming without obvious pump.
- For **lead guitar sustain**, set a low threshold and moderate ratio (4:1–6:1) with a medium release. The compressor keeps the note level up as it naturally decays.
- **Parallel compression (Mix below 100%)** is excellent for keeping the natural dynamics of your playing while still controlling the overall level. Try Mix at 60–80%.
