# Auto Arpeggiator

> BPM-synced rhythmic arpeggiator — cycles through a chord interval pattern by pitch-shifting your signal on each beat, turning a single note into an automatic arpeggio.

## What it does

The Auto Arpeggiator takes your guitar signal and rhythmically steps through a set of pitch-shifted intervals in time with the song tempo. On each step, your entire signal is pitch-shifted to the next interval in the pattern — so playing a single held note produces an automatic arpeggio. You choose the harmonic pattern (major triad, minor triad, power chord, octaves, or custom), the step rhythm, and the direction the arpeggio moves.

The arpeggiator is **tempo-synced** — it reads the song BPM from the DAW (or from the app's metronome) and locks the step timing to musical note values (quarter notes, eighth notes, etc.).

A **gate envelope** controls the shape of each note: the attack fades the note in, the gate portion holds it at full level, and the release fades it out before the next step — preventing clicks and controlling the articulation feel.

## Parameters

### Timing

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Step Rate** | 1/4, 1/8, 1/16, 1/8 Triplet, 1/16 Triplet | 1/8 Note | The note value of each arpeggio step relative to the song tempo. 1/8 = eighth-note steps; 1/16 = faster sixteenth-note steps |

### Pattern

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Pattern** | Major Triad, Minor Triad, Power Chord, Octaves, Custom | Major Triad | The set of intervals the arpeggiator cycles through. Major Triad = root, major third, fifth, octave (+0, +4, +7, +12 semitones); Power Chord = root and fifth only |
| **Direction** | Up, Down, Up-Down | Up | The order in which the pattern intervals are played. Up = ascending; Down = descending; Up-Down = ascending then back down |
| **Steps** | 2–8 | 4 | The number of active steps in **Custom** mode. In non-custom patterns, this is set automatically by the pattern |

### Gate envelope

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Gate** | 5–100% | 80% | The fraction of each step during which the arpeggio note is audible (at full level). 80% = the note is held for 80% of the step duration, then released. Lower = more staccato; 100% = legato (note held the full step) |
| **Attack** | 0–50% | 5% | The ramp-up time at the start of each step, as a fraction of the step duration. Short Attack = abrupt; longer = a slight swell into each note |
| **Release** | 0–50% | 8% | The fade-out time at the end of each gate, preventing clicks as the note cuts off |

### Custom steps *(active in Custom pattern mode only)*

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Step 1–8** | −24 to +24 semitones | 0, 4, 7, 12, 0, 0, 0, 0 | The pitch interval for each step, in semitones relative to the input signal. 0 = root (no shift); +4 = major third; +7 = fifth; +12 = octave |

### Pitch trigger *(optional — controls when the arp is active)*

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Pitch Trigger** | Always, Above, Below | Always | Controls when the arpeggiator engages. **Always** = arpeggiator is always on. **Above** = arpeggiator only activates when your playing pitch is above the Pitch Threshold (e.g. only arpeggiate high notes). **Below** = arpeggiator only activates below the threshold (e.g. only arpeggiate bass notes) |
| **Pitch** | 50–2000 Hz | 330 Hz | The frequency threshold used by the Above/Below trigger modes. 330 Hz is approximately E4 (open high E string) |

### Mix

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **Mix** | 0–100% | 80% | Blends the arpeggiated (wet) signal with the dry guitar signal. 100% = only arpeggiated notes; lower Mix blends the original dry signal underneath |

## Tips

- **Power Chord pattern, 1/8 note steps, Direction Up** is a great starting point for rock/metal style arpeggio runs — root then fifth, repeating on every eighth note.
- **Custom pattern + Steps 4–6** gives you full control. Try 0, 4, 7, 0, 4, 7 for a looping major triad; or 0, 5, 7, 12 for a suspended/shell voicing.
- **Gate below 60%** creates a punchy, staccato arpeggio with noticeable space between notes — great for rhythmic patterns. **Gate at 95–100%** creates a smooth, legato arpeggio where notes nearly overlap.
- **Step Rate 1/16 + Gate ~50%** produces a fast, mechanical arpeggio style common in EDM and trance. **1/4 note + Gate 80%** is slower and more melodic.
- **Pitch Trigger "Above" at 330 Hz** is useful when you want the arpeggiator to kick in only on lead-register notes while lower chord voicings play through clean and unaffected.
- Because the arpeggiator uses pitch-shifting, it works best on clean or lightly driven signals. Very high gain may make the pitch shifted intervals sound muddy — try placing the arp before the amp model.
