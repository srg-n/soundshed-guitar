# Auto-Wah

> A filter that opens and closes with your picking attack — a "wah pedal" driven by how hard you play instead of your foot.

## What it does

The Auto-Wah uses an envelope follower to track the loudness of your playing and maps that to a filter sweep. When you pick hard, the filter opens up (sweeping towards a higher, brighter frequency); when the note decays or you pick softly, the filter closes back down (rolling off the high frequencies). The effect mimics the expressive character of a wah pedal, but responds automatically to your playing dynamics rather than requiring a foot controller. It is central to funk and soul guitar but works in many other contexts.

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Sensitivity** | 0–100% | 60% | How much of your picking dynamics affects the filter sweep. Low Sensitivity = only the hardest picks trigger a full open sweep; high Sensitivity = even soft notes trigger a pronounced sweep |
| **Min Frequency** | 200–1000 Hz | 300 Hz | The lowest frequency the filter rests at when the envelope is closed (quiet playing). This sets the "closed wah" position |
| **Max Frequency** | 800–5000 Hz | 2800 Hz | The highest frequency the filter reaches when the envelope is open (hard attack). This sets the "open wah" position |
| **Resonance** | 0.5–10 Q | 2.5 | The sharpness and emphasis of the filter peak. Low Resonance = broad, gentle filter sweep; high Resonance = a more pronounced "quack" with a stronger peak at the filter frequency |
| **Mix** | 0–100% | 100% | Blends the filtered (wet) signal with the dry. Reducing Mix softens the effect while retaining some of the dry tone |

## Tips

- **Sensitivity is the most important control for feel.** Set it so that normal picking triggers the sweep, but you can still choke it down by playing softly. Start at 50% and adjust.
- **Higher Resonance = more "quack"** — the classic funk wah character. Try 3–5 Q for pronounced vowel-like tones.
- **Widen the Min/Max Frequency range** for a more dramatic sweep. Min at 250 Hz and Max at 3000 Hz produces a wide, satisfying sweep.
- **The attack speed is fixed at ~5 ms (fast) and the release at ~80 ms (moderate).** The fast attack means the filter opens quickly on each picked note, then closes slowly as the note decays — this is what gives the "chick-a" character.
- Auto-Wah works best with dynamic picking — dig in hard on the accented beats for the most expressive response.
- Works well before or after the amp. Before the amp: the filtered tone goes through the amp, which colours the wah with amp saturation. After: the amp tone is then filtered, which can make the wah more dramatic.
