# Synth Saw

> Converts your guitar signal into a sawtooth synthesiser voice — guitar-to-synth tracking in real time.

## What it does

The Synth Saw tracks the pitch of your guitar and uses it to drive a sawtooth-wave synthesiser oscillator. Instead of hearing your guitar, you hear a bright, buzzy synth tone that follows every note you play. It is the classic "guitar synth" effect — turning your guitar into a lead or bass synthesiser. A second independent oscillator voice (Voice 2) can be mixed in at a different pitch for thicker, harmony-layered textures.

The **Mix** control lets you blend the synth voice with the original guitar signal, so you can have anything from a subtle synth layer underneath your natural guitar tone to a pure synth-only sound.

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Mix** | 0–100% | 100% | Blends the synthesised signal with the dry guitar signal. 100% = synth only; 50% = equal guitar + synth |
| **Attack** | 0.1–100 ms | 5 ms | How quickly the synth voice fades in when a new note is detected. Shorter Attack = abrupt onset; longer Attack = a soft swell into each note |
| **Release** | 10–1000 ms | 100 ms | How long the synth voice sustains after the guitar signal falls silent. Short Release = tight, staccato notes; long Release = the synth voice blooms and lingers |
| **Detune** | −100 to +100 cents | 0 cents | Shifts the synth oscillator slightly above or below the detected pitch in cents (100 cents = 1 semitone). Small amounts (±5–15 cents) thicken the tone; larger amounts create intentional detuning |
| **Octave** | −2 to +2 oct | 0 | Shifts the synth voice up or down by one or two octaves. −1 gives a bass-guitar-like sub voice; +1 gives a bright, high register voice |
| **Glide** | 0–500 ms | 10 ms | The time taken to slide between detected pitches (portamento). 0 ms = immediate pitch change; longer Glide = a smooth slide between notes, like legato synth playing |
| **Output** | −24 to +12 dB | 0 dB | Final output level of the synth effect |
| **Gate** | −80 to 0 dB | −60 dB | The input level threshold below which the synth voice is silenced. Acts like a noise gate — signals below the Gate level produce no synth output. Raise the Gate if the synth keeps triggering from amp noise |

### Voice 2 (second oscillator)

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Voice 2 Pitch** | −24 to +24 semitones | 0 st | Pitch offset of the second oscillator voice, in semitones, relative to the detected note. 0 = unison (both voices on the same note); +7 = a fifth above; +12 = an octave above |
| **Voice 2 Mix** | 0–100% | 0% | Level of the second oscillator voice. 0% = Voice 2 is silent. Raise to blend in the harmony voice |

## Tips

- **For the cleanest pitch tracking**, play single notes with clear, well-defined attacks. Chords and fast runs with overlapping notes will confuse the pitch detector and produce glitching artefacts.
- **Lower the Gate slightly** if your setup is very clean and quiet — the default −60 dB is quite conservative and should work for most setups, but lower it further if the synth seems to drop out on light picking.
- **Glide at 30–80 ms** adds an expressive portamento between notes when playing legato-style lines — very effective for lead synth work.
- **Voice 2 at +12 semitones (Octave up) with Voice 2 Mix at 40–60%** produces a classic octave synth pad — the root voice and its octave create an instant harmoniser effect.
- **Voice 2 at +7 semitones (fifth) with Voice 2 Mix at 30%** adds a power-chord-style harmony to every note you play.
- Place Synth Saw **after the amp and cabinet** to hear the synth character purely, or **before the amp** to run the synth voice through the amp model for a dirtier, more amp-influenced synth tone.
