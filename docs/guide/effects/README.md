# Effects Reference

This section documents every effect available in Soundshed Guitar. Each page covers what the effect does, what every parameter controls, and practical tips for getting the best results.

---

## How to add an effect

1. Open the **Signal Chain Editor** tab.
2. Click an effect category in the **FX Browser** on the left.
3. Click the effect name to add it to the end of your chain, or drag it into position.

---

## Effect categories

### Amplifiers
Amp effects model the core amplifier character of your tone — the most important piece in the chain.

| Effect | Description |
|--------|-------------|
| [Neural Amp Model (NAM)](amp-nam.md) | Loads a neural capture of a real guitar amplifier |
| [NAM Blend](amp-nam-blend.md) | Mixes two or more NAM models together |
| [Built-in Amp](amp-builtin.md) | Classic "Heavy American" preamp with full tone stack and power stage controls |

### Cabinets
Cabinet effects simulate the speaker cabinet and microphone, completing the amp tone.

| Effect | Description |
|--------|-------------|
| [IR Cabinet](cab-ir.md) | High-quality cabinet simulation using an Impulse Response file |
| [Simple Cabinet](cab-simple.md) | Lightweight cabinet shaping with bass, presence, and brightness controls |

### Dynamics
Dynamics effects control the volume envelope of your signal — quieting noise, controlling sustain, or preventing clipping.

| Effect | Description |
|--------|-------------|
| [Noise Gate](noise-gate.md) | Silences the signal when it falls below a threshold — kills amp hum and hiss between notes |
| [VCA Compressor](compressor-vca.md) | Fast, punchy compressor — tightens attack and adds sustain |
| [Opto Compressor](compressor-opto.md) | Smooth, musical compressor with a slower, more transparent response |
| [Brickwall Limiter](limiter.md) | Hard ceiling that prevents the output from ever going above a set level |

### Drive & Saturation
Drive effects add grit and harmonic richness by pushing the signal into clipping.

| Effect | Description |
|--------|-------------|
| [Overdrive](overdrive.md) | Soft, warm clipping — like a transparent boost into a tube amp |
| [Distortion](distortion.md) | Hard, aggressive clipping with more sustain and edge |
| [Fuzz](fuzz.md) | Extreme, woolly saturation inspired by vintage fuzz pedals |

### EQ
EQ shapes the tonal balance by boosting or cutting specific frequency ranges.

| Effect | Description |
|--------|-------------|
| [Parametric EQ](eq-parametric.md) | Four independent bands with adjustable frequency, gain, and Q (bandwidth) |

### Delay
Delay effects create echoes by feeding a delayed copy of the signal back into the mix.

| Effect | Description |
|--------|-------------|
| [Digital Delay](delay-digital.md) | Clean, tempo-based delay with adjustable time, feedback, and mix |
| [Stereo Doubler](delay-doubler.md) | Very short delay that widens the stereo image and thickens the tone |

### Reverb
Reverb effects simulate acoustic spaces — from small rooms to large halls and beyond.

| Effect | Description |
|--------|-------------|
| [Reverb (all types)](reverb.md) | Room, Hall, Plate, Chamber, Spring, Shimmer, Ambient, Advanced, and IR Convolution reverbs |

### Modulation
Modulation effects add movement and animation to the tone through periodic changes in pitch, phase, or volume.

| Effect | Description |
|--------|-------------|
| [Chorus](chorus.md) | Thickens the sound by mixing a slightly detuned, delayed copy |
| [Flanger](flanger.md) | A sweeping, jet-like comb filtering effect |
| [Phaser](phaser.md) | Swirling, phase-cancellation effect — smooth and cyclical |
| [Tremolo](tremolo.md) | Rhythmic volume pulsing |
| [Auto-Wah](auto-wah.md) | Envelope-controlled filter that opens and closes with your picking attack |
| [Auto Arpeggiator](auto-arp.md) | BPM-synced arpeggiator that steps through chord intervals in time with the tempo |

### Pitch
Pitch effects shift or harmonise the pitch of your signal.

| Effect | Description |
|--------|-------------|
| [Pitch Shift](pitch-shift.md) | Shifts pitch up or down by up to 12 semitones with optional mixing |
| [Transpose](transpose.md) | Integer semitone transposition for drop tunings and capo simulation |
| [Octave](octave.md) | Blends octave-up and octave-down voices with the dry signal |

### Utility
Utility nodes control signal level and routing.

| Effect | Description |
|--------|---------|
| [Gain Stage](gain.md) | Simple gain adjustment at any point in the chain |

### Synth
Synth effects convert your guitar signal into a synthesiser-style sound.

| Effect | Description |
|--------|---------|
| [Synth Saw](synth-saw.md) | Guitar-to-synth voice using a sawtooth oscillator driven by your picking |
