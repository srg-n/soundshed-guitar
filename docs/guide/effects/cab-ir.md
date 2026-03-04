# IR Cabinet

> Simulates a guitar speaker cabinet and microphone using an Impulse Response file.

## What it does

An Impulse Response (IR) is a recording of how a specific speaker cabinet, room, and microphone combination sounds. When you load an IR into the IR Cabinet node, the effect mathematically applies that cabinet/mic character to your amp signal — giving you the sound of being in a real recording session with that cabinet in front of the mic. The quality of the IR Cabinet can be set from Economy (less CPU) through to Full (highest fidelity).

## Parameters

| Parameter | Range | Default | What it does |
|-----------|-------|---------|--------------|
| **IR File** | (resource picker) | — | The impulse response file to use as the cabinet. Click the folder icon to browse your library |
| **Mix** | 0–100% | 100% | Blends between the dry (no cabinet) signal and the fully processed (cabinet) signal. Keep at 100% in most cases |
| **Output Gain** | −24 to +24 dB | 0 dB | Adjusts the level after cabinet processing |
| **Quality** | Economy / Standard / High / Full | Standard | Controls the length of the impulse response used. Higher quality sounds better but uses more CPU |

## Quality settings explained

| Setting | Character |
|---------|-----------|
| Economy | Shortest processing — fine for monitoring, may lose some low-end weight and air |
| Standard | Good balance of quality and CPU — suitable for most situations |
| High | Very close to the full IR — most users will not hear a difference from Full |
| Full | Complete impulse response — maximum fidelity, highest CPU cost |

## Tips

- **Try multiple IRs before committing** — the same amp model can sound radically different with different cabinet IRs. A classic 4×12 Celestion stack, a vintage 2×12 combo, and a single 1×10 speaker will all give completely different characters from the same amp model.
- **Mix at 100%** in almost all cases. Blending in dry signal from a modelled amp without a cabinet usually sounds hollow and unnatural. The only time you'd reduce mix is if you are blending two parallel paths where one has a cabinet.
- **Economy mode** is useful when CPU is tight during live use; switch to Standard or High for final recording.
- If an IR sounds boomy or muddy, try a different mic position IR of the same cabinet — the microphone position makes a huge difference.
