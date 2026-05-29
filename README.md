# Dynamic EQ — JUCE VST3/AU Plugin

A 3-band dynamic EQ with full per-band dynamics, sidechain support, envelope following,
and full MIDI/automation control. Builds as VST3, AU, and Standalone.

---

## Features

### Bands
| Band | Default Freq | Default Type |
|------|-------------|--------------|
| Low  | 200 Hz      | Low Shelf    |
| Mid  | 1000 Hz     | Bell         |
| High | 5000 Hz     | High Shelf   |

Each band has: Frequency, Gain (±18 dB), Q, Filter Type, Bypass.

### Dynamics (per band)
- **Threshold** — level at which gain reduction starts
- **Ratio** — compression/expansion ratio (1:1 = off, >1 = compress, <1 = expand)
- **Attack / Release** — envelope follower timing
- **Knee** — soft knee width (0 = hard knee)
- **Makeup Gain** — compensate for gain reduction
- **Max Boost / Max Cut** — safety clamps on dynamic range

### Detection Modes (per band)
| Mode       | Description                                                       |
|------------|-------------------------------------------------------------------|
| Internal   | Envelope follower on the band-filtered signal                     |
| Sidechain  | Feed an external signal into Bus 2 to drive the EQ dynamics      |
| Manual     | Host automation / MIDI controls the gain reduction directly       |

### Spectrum Analyser
Real-time FFT spectrum display (2048-point, Hann window, log-freq scale).

---

## Building

### Prerequisites
- **CMake** 3.22 or later
- **C++17** compiler (Clang 12+, GCC 11+, MSVC 2022+)
- **git** (FetchContent downloads JUCE automatically)
- On Linux: `sudo apt install libasound2-dev libfreetype-dev libx11-dev libxinerama-dev libxrandr-dev libxcursor-dev mesa-common-dev`

### Linux / macOS
```bash
chmod +x build.sh
./build.sh Release
```

Output: `build/DynamicEQ_artefacts/Release/VST3/DynamicEQ.vst3`

### Windows (MSVC)
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Install to Ableton Live
**VST3 paths:**
- Windows: `C:\Program Files\Common Files\VST3\`
- macOS:   `~/Library/Audio/Plug-Ins/VST3/`
- Linux:   `~/.vst3/`

**AU path (macOS only):**
- `~/Library/Audio/Plug-Ins/Components/`

Then in Ableton: Preferences → Plug-Ins → Rescan.

---

## Using in Ableton

### Sidechain
1. Load DynamicEQ on your target track.
2. In Ableton's device chain, enable the **Sidechain** input (dropdown at top of device).
3. Route any track to that sidechain input.
4. Set the band's **Detect** mode to **Sidechain**.

### MIDI Automation
- All parameters are automatable in Live's clip/session automation lanes.
- **Manual GR** param lets you draw in gain reduction per band directly.
- MIDI-map any parameter: `MIDI Map Mode → click knob → move controller`.

### Envelope Follower (Internal)
- Set **Detect** to **Internal** (default).
- Set Threshold so the band reacts when that frequency range gets loud.
- Low ratio (1.5–2) = gentle ducking. High ratio (8–20) = aggressive clamp.

---

## Architecture

```
Audio In (Stereo)
    │
    ├── [Low Band]  LowShelf filter + dynamics (envelope → gain mod)
    ├── [Mid Band]  Bell filter    + dynamics
    └── [High Band] HighShelf filter + dynamics
            │
         Output Gain
            │
         Audio Out + FFT Analyser → GUI
```

Each band:
```
Input → Detection Filter → Envelope Follower ─→ Gain Computer
  │                                                    │
  └────────────────────── EQ Filter (gain = static + dynamic) → Output
```

---

## Extending

- **Add bands**: Increase `NUM_BANDS`, add default freqs, create params in loop.
- **Mid/Side mode**: Split buffer into M/S before band processing.
- **Lookahead**: Add a circular delay buffer on the main signal path; let detector run N samples ahead.
- **Linear phase**: Swap `IIR::Filter` for `FIR::Filter` or use `dsp::Oversampling`.
