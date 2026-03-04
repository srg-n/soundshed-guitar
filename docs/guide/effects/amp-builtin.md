# Built-in Amp ("Heavy American")

> A fully featured built-in amplifier model with voiced preamp, tone stack, power section, and speaker simulation — no external model file required.

## What it does

The Built-in Amp is an internally modelled amplifier covering everything from a clean, glassy tone to a saturated, modern high-gain sound. Unlike the [Neural Amp Model (NAM)](amp-nam.md), it requires no external model file — the full amplifier circuit is built in. It models a multi-stage preamp with independent tone controls, a power amp section, and a speaker simulator, giving you a complete amp-in-a-box experience.

The **Voice** control switches between two distinct amplifier characters: clean and drive. The **Gain** control drives the preamp harder for more saturation and sustain.

## Parameters

### Input section

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Voice** | 0 or 1 | 0 (Clean) | Switches between Clean (0) and Drive (1) preamp character. Drive voice adds more preamp clipping and compression |
| **Gain** | 0–100% | 45% | Preamp drive level. Higher Gain = more saturation, sustain, and compression |
| **Bright** | 0 or 1 | Off | Toggles a high-shelf brightness boost (~2500 Hz) in the preamp. Adds sparkle and clarity on clean tones |
| **Pre Emphasis** | 0–100% | 0% | Adds a treble emphasis before the preamp stages (+0–6 dB shelf at ~2500 Hz). Brightens the input and slightly increases gain response to picking attack |

### Preamp section

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Preamp Stages** | 1–4 | 2 | Number of gain stages in the preamp. More stages = more saturation and more compression. 2 is the standard character; 4 produces extreme saturation |
| **Stage Gain** | −24 to +24 dB | 0 dB | Gain applied at each preamp stage. Raising this drives the stages harder for more distortion character |

### Tone stack

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Bass** | 0–100% | 50% | Low-frequency shelf (±9 dB around centre). Higher Bass = more warmth and body |
| **Middle** | 0–100% | 50% | Midrange peak at ~750 Hz (±9 dB). The "heart" vocal character of the amp |
| **Treble** | 0–100% | 50% | High-frequency shelf (±9 dB around ~3500 Hz). Higher Treble = more bite and clarity |
| **Contour** | 0–100% | 20% | Cuts the midrange at ~600 Hz for a scooped sound. Higher Contour = more scoop — the classic "V-shaped" EQ favoured in heavy metal |
| **Presence** | 0–100% | 50% | Boosts the upper midrange at ~4000 Hz (±6 dB). Higher Presence = more cut and attack in the tone |

### Output section

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Output** | −24 to +24 dB | 0 dB | Output level after the entire amp model |

### Power amp section *(advanced)*

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Power Drive** | 0–100% | 0% | Adds power amplifier saturation — the compression and soft clipping that happens in a real amp's output stage when it is pushed hard |
| **Sag** | 0–100% | 0% | Simulates power supply sag — the momentary voltage drop under heavy pick attack in a real amp. Creates a slight "give" in the feel |
| **Bias** | −100% to +100% | 0% | Adjusts the bias point of the power stage. Positive bias = hotter, brighter character; negative bias = cooler, darker character |

### Speaker section *(advanced)*

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Depth** | 0–100% | 40% | Low-frequency shelf boost (below ~120 Hz) that simulates the bass resonance of a speaker cabinet |
| **Resonance** | 0–100% | 40% | Adds a resonant peak at the speaker cabinet's bass resonance frequency (~120 Hz). Increases the "thump" on each note |
| **Damping** | 0–100% | 50% | High-frequency shelf cut (above ~3500 Hz) simulating how the speaker cone naturally rolls off highs. Higher Damping = darker, more rolled-off tone |

## Tips

- **Voice 0 (Clean) + Gain 25–40%** gives a glassy, touch-sensitive clean tone. Roll back the guitar volume for complete transparency.
- **Voice 1 (Drive) + Gain 50–70%** gives a saturated crunch; **Gain 80–100%** with 3–4 Preamp Stages gives a thick high-gain sound.
- **Contour** is the most dramatic tonal shaper. Keep it below 30% for classic voiced tones; raise to 60–80% for the American scooped-mid heavy sound.
- Unlike NAM models, the Built-in Amp already models a speaker response via the Speaker section — you may not need an IR Cabinet after it. If you do add an IR Cabinet, reduce the Built-in Amp's Damping to 0% first so the two cabinet simulations do not stack.
- **Sag and Power Drive together** produce the feel of a real pushed tube amp — start with Power Drive at 20–30% and Sag at 20%, and adjust to taste.
