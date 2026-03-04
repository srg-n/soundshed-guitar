# The Signal Chain

The signal chain is the path your guitar signal travels through — from the moment it enters the app to the moment it reaches your speakers. Understanding how it works helps you build better tones and troubleshoot problems.

---

## How signal flows

Think of the signal chain like a traditional pedalboard on the floor:

```
Guitar → [Noise Gate] → [Amp Model] → [Cabinet IR] → [Reverb] → Speakers
```

Each block in the chain is called a **node**. The signal enters the first node, gets processed, then passes on to the next — left to right, top to bottom in the visual editor.

---

## Why order matters

The order of effects has a big impact on your tone:

- A **noise gate before the amp** cuts hum and hiss before it gets amplified and distorted. A gate after the amp would have to deal with a much louder, dirtier signal.
- A **drive pedal before the amp** pushes the amp model harder (like a boost pedal into a real amp). After the amp, it would just add clipping on top of what the amp already does.
- **Reverb and delay after the amp** sound natural because real cabs are mic'd in real rooms. Reverb before the amp would wash out the clarity of the amp tone.

A typical arrangement:

```
Guitar → Gate → Drive → Amp → Cabinet → EQ → Delay → Reverb
```

---

## Global chain

Soundshed Guitar has a **global pre-chain** (applied before the preset, e.g. noise gate) and a **global post-chain** (applied after, e.g. EQ, doubler). These are consistent across all presets and are configured in Settings.

---

## Bypass

Every node in the chain has a **bypass toggle**. When bypassed, the signal passes straight through that node unchanged — the node is skipped. This lets you audition what a single effect is contributing without removing it from the chain.

---

## Parallel paths

The chain does not have to be purely linear. You can split your signal into two or more parallel paths using a **Splitter** node, then merge them back with a **Mixer** node:

```
               ┌─→ [Effects] ─→┐
Amp ─→ Splitter─┤                 ├─→ Mixer ─→ Output
               └─→ [Effects] ─→┘
```

This is useful for blending two different amp sounds, or running a dry signal in parallel with a heavily effected wet signal.

---

## Input and output trim

Two global controls affect the entire chain:

- **Input Trim** — scales the signal level coming into the chain (−40 to +20 dB). Use this if your guitar is too quiet or too hot going into the amp model.
- **Output Trim** — scales the final output level (−40 to +20 dB). Use this to match levels between presets.

---

## Continue reading

- [Signal Chain Editor](signal-chain-editor.md) — how to add, remove, and rearrange nodes visually
