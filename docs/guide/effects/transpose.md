# Transpose

> Shifts your entire signal up or down by a set number of semitones — for drop tunings, capo simulation, and key changes.

## What it does

Transpose shifts the pitch of your signal by a fixed number of whole semitones — no fractional values. It is designed for practical tuning tasks: simulating a capo (shift up), playing in a lower tuning without retuning your guitar (shift down), or quickly transposing a part to a different key for a singer. The processing is optimised for integer semitone shifts to give the cleanest possible result.

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Semitones** | −36 to +12 st | 0 | The number of semitones to shift the signal. Negative values = shift down; positive = shift up. −12 = one octave down; −24 = two octaves down; +12 = one octave up |
| **Mix** | 0–100% | 100% | Blends the transposed (wet) signal with the dry. At 100% (typical) you only hear the transposed pitch. Reducing Mix blends dry pitch with the shifted pitch for a hybrid sound |

## Common use cases

| Task | Setting |
|------|---------|
| Simulate drop D tuning | −2 semitones |
| Simulate drop C tuning | −4 semitones |
| Simulate a capo at fret 2 | +2 semitones |
| Play an Eb guitar part on a standard-tuned guitar | −1 semitone |
| Simulate a 7-string baritone range | −7 to −12 semitones |

## Tips

- Keep **Mix at 100%** for most uses — Transpose is a utility effect, not typically a creative blend effect. Blending at 50% produces parallel octave/fifth harmony similar to [Pitch Shift](pitch-shift.md).
- **Large downward shifts (−12 to −36 semitones) introduce audible artefacts** for fast or complex chord playing. They work best on single-note lines and simple chords.
- At −12 semitones, the result is similar to the [Octave](octave.md) effect's octave-down voice — but Transpose produces the full signal shifted down rather than a blended effect.
- Use Transpose in combination with your guitar's full standard tuning when you need to quickly play along with a recording in a different key — faster than retuning your guitar.
