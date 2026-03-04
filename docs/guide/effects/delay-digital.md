# Digital Delay

> Clean echo effect that repeats your signal after a set time — from tight slapback to long rhythmic echoes.

## What it does

The Digital Delay creates one or more copies of your signal, delayed by a set amount of time, and mixes them in with your dry signal. The result ranges from a punchy slapback echo (very short delay, one or two repeats) to long, cascading rhythmic trails that wash behind your playing. The repeats are clean and transparent — faithful copies of the original signal.

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Time** | 1–2000 ms | ~300 ms | How long after the original note each echo occurs. 50–100 ms = slapback; 200–500 ms = rhythmic echo; 500–2000 ms = long, ambient trail |
| **Feedback** | 0–95% | ~30% | How much of the delayed signal is fed back into the delay line to create additional repeats. Low Feedback = 1–2 echoes that die quickly; high Feedback = many cascading repeats; 95% = very long trail that dies gradually |
| **Mix** | 0–100% | ~40% | Blends the delayed (wet) signal with the dry signal. At 100% you only hear the echoes, not the dry note |

## Rhythmic delay timing

To lock delay to a musical tempo:

| Notes at 120 BPM | Time (ms) |
|-----------------|-----------|
| Quarter note | 500 ms |
| Dotted eighth | 375 ms |
| Eighth note | 250 ms |
| Triplet eighth | 167 ms |

A formula for quarter-note delay time in milliseconds: **60,000 ÷ BPM**.

## Tips

- **Slapback for rockabilly/country:** Time ~80–120 ms, Feedback 0–10% (just one or two repeats), Mix 20–30%. This adds depth and space without audible separate echoes.
- **Dotted-eighth delay for classic rock/country lead:** Set Time to the dotted-eighth value for your tempo (e.g. 375 ms at 120 BPM), Feedback 20–30%, Mix 25–35%. Picking on every beat creates rhythmic trails between the notes that feel like the delay is "playing" with you.
- **Long ambient trail:** Time 600–1200 ms, Feedback 50–70%, Mix 20–25%. The echoes build slowly behind your playing without drowning the dry signal.
- **Keep Mix low (20–35%) in most cases.** Too much delay Mix makes the dry signal sound weak and the echoes muddy in a band context.
- Place delay **after the amp and cab** in the chain, and **before reverb** so the echoes trail into reverb rather than the other way around.
