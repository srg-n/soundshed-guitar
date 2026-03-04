# Noise Gate

> Silences your signal when it falls below a set volume threshold — cuts amp hum and string noise between notes.

## What it does

A noise gate works like an automatic mute. When your signal is loud (you are playing), the gate opens and lets the signal through. When your signal drops below the threshold (you have stopped playing or lifted your fingers), the gate closes and silences everything — including hum, buzz, and amp noise. This is especially useful for high-gain tones where the amp amplifies noise as aggressively as it amplifies your guitar.

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Threshold** | −80 to 0 dB | (low) | The volume level at which the gate opens. Signals above this level pass through; signals below are muted. Raise it to cut more noise; lower it to avoid cutting off quiet notes |
| **Attack** | 0.1–50 ms | ~5 ms | How quickly the gate opens once the signal exceeds the threshold. Faster attack (low value) = abrupt open; slower attack = smoother, more gradual opening |
| **Release** | 1–500 ms | ~100 ms | How quickly the gate closes after the signal drops below the threshold. Shorter release cuts off ringing strings faster; longer release lets notes decay more naturally |

## Tips

- **Place the noise gate before the amp model** in the chain. Gating before the amp means the gate only needs to deal with the relatively quiet guitar noise — after the amp, the amp's own hum is also in the signal, making it harder to gate cleanly.
- **Set the threshold just above your noise floor.** Play a note, let it ring out, and listen for where the amp noise becomes audible as the note dies away. Set the threshold just above that level.
- **Start with a slow release (~100 ms)** when using a high-gain tone. Too short a release causes the gate to chatter — it flickers open and closed on sustained notes. Increase release until the gate closes smoothly without chopping off notes.
- A fast attack (2–5 ms) is fine for most playing. Only slow it down if you hear a slight "thump" as the gate opens on picked notes.
