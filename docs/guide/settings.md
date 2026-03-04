# Settings

The Settings page controls how Soundshed Guitar connects to your audio hardware, where it looks for models and IRs, how it displays, and various advanced options.

---

## Opening settings

Click the **gear icon** in the top-right corner of the app.

---

## Audio Device

| Setting | What it does |
|---------|--------------|
| **Audio Device** | Selects your audio interface or built-in audio device |
| **Input Channel** | Which socket on your interface your guitar is plugged into |
| **Output Channels** | Which outputs go to your monitors or headphones |
| **Sample Rate** | The audio sample rate (44100, 48000, 96000 Hz). Match your DAW's project rate if using as a plugin |
| **Buffer Size** | How many samples are processed at once. Lower = less delay (latency) but higher CPU load. 128 is a good starting point |

Click **Apply** after making changes. The audio engine restarts briefly.

---

## Resource Folders

Soundshed Guitar searches these folders when building the library of models and IRs:

- Click **Add Folder** to include a folder on your computer (e.g. where you store your NAM model downloads).
- Click **Remove** to stop watching a folder.
- Click **Rescan** to refresh the library after adding new files manually.

---

## NAM Calibration

Neural Amp Models carry metadata indicating what input level they were trained at. The calibration setting aligns this to your setup so the amp behaves as intended.

| Setting | What it does |
|---------|--------------|
| **Auto-Level** | When enabled, input gain is adjusted automatically based on the model's calibration data. Leave this on unless you have a specific reason to disable it |
| **Reference Level** | The dBu level that corresponds to 0 dBFS at your interface's input. Default is +12.0 dBu. Change this if your interface has a different headroom specification |

If amps sound too compressed or too quiet, try adjusting the Reference Level up or down a few dB.

---

## Theme

Choose from five visual themes:

| Theme | Character |
|-------|-----------|
| **Default** | Modern, neutral dark interface |
| **Light** | Bright, clean look for well-lit rooms |
| **Dark** | Deep blacks for low-light environments |
| **Classic 70s** | Warm browns and amber tones |
| **Worn Pedal** | Weathered look inspired by vintage hardware |

Click a theme to apply it immediately.

---

## Tone3000 API Key

Enter your Tone3000 API key here to enable community model browsing, downloads, and ratings. See [Community & Tone3000](community.md) for details on obtaining a key.

---

## Diagnostics

| Control | What it does |
|---------|--------------|
| **CPU Load display** | Shows real-time CPU usage per audio block; identifies which effects are most expensive |
| **Signal Level Diagnostics** | Streams live input and output levels per node in the chain — useful for finding where a signal is too hot or too quiet |
| **Run Signal Path Test** | Sends a test tone through the chain and reports any nodes that are not producing output |

---

## Tips

- If you experience audio dropouts (crackles, glitches), increase the buffer size by one step (e.g. 128 → 256).
- If the latency feels sluggish, try reducing the buffer size one step at a time until dropouts reappear, then step back up.
