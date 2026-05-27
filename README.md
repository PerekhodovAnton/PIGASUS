# Pigasus

A saturator + brick-wall limiter VST3 / AU audio plugin, built with JUCE.

Push the **Drive** knob and the plugin cooks the signal harder — auto-makeup gain keeps the perceived loudness (LUFS) roughly constant while peaks stay tamed. Four saturation flavors (Tube, Tape, Diode, Hard). A rock-pig mascot in the gauge gets progressively angrier the harder you push, with flames erupting from the gauge ring at high drive.

---

## Install (build from source)

You need: **Git** and **CMake**.
On macOS you also need the Xcode Command-Line Tools.
On Windows you also need **Visual Studio Build Tools** (Desktop development with C++).

The first build downloads JUCE automatically (~150 MB) — after that, builds are fast.

### macOS

Open Terminal and run:

```sh
# One-time install of build tools (skip if you already have them)
xcode-select --install
brew install cmake git

# Get the source and build
git clone https://github.com/PerekhodovAnton/PIGASUS.git
cd PIGASUS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

The plugin is **automatically installed** to:

- `~/Library/Audio/Plug-Ins/VST3/Pigasus.vst3`
- `~/Library/Audio/Plug-Ins/Components/Pigasus.component` (AU)

Open Ableton / Logic / Reaper / your DAW. **Pigasus** should appear in the plugin list (you may need to rescan).

### Windows (easy — just run the installer)

1. Download the latest **`Pigasus-X.X.X-Setup.exe`** from the [Releases page](https://github.com/PerekhodovAnton/PIGASUS/releases).
2. Double-click it. Click **Yes** when Windows asks for admin (needed to install into the system VST3 folder), then **Next → Install → Finish**.
3. Open your DAW → *Rescan plugins* → **Pigasus** should appear in the list.

That's it. The installer puts `Pigasus.vst3` into `C:\Program Files\Common Files\VST3\` and comes with a proper uninstaller in *Add or remove programs*.

> If Windows SmartScreen warns "Windows protected your PC", click **More info → Run anyway**. The installer isn't code-signed yet (signing costs a yearly fee) but is safe — it's built by the public CI workflow in this repo.

### Windows (manual build, for developers)

If you want to build it yourself:

1. Install **Git**: <https://git-scm.com/download/win>
2. Install **CMake**: <https://cmake.org/download/> (tick *"Add CMake to system PATH"*)
3. Install **Visual Studio Build Tools 2022** with the **"Desktop development with C++"** workload: <https://visualstudio.microsoft.com/downloads/?q=build+tools>

```powershell
git clone https://github.com/PerekhodovAnton/PIGASUS.git
cd PIGASUS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The compiled plugin will be at `build\Pigasus_artefacts\Release\VST3\Pigasus.vst3`. Copy that folder into `C:\Program Files\Common Files\VST3\`.

---

## How to use it

1. Drop Pigasus on any audio track (bass, vocals, drums — anything that benefits from saturation).
2. Pick a saturation mode with the four pedal-style buttons: **tube** (warm), **tape** (bronze), **diode** (electric), **hard** (aggressive).
3. Push the **Drive** knob (0 → 12 dB). Watch the gauge needle climb into the redline zone; the pig will change mood and flames will start curling around the dial.
4. The **Pressure** bar at the bottom shows how hard the plugin is working overall.

---

## Project layout

```
PIGASUS/
├── CMakeLists.txt          ← build config
├── Source/                 ← C++ source for the plugin
│   ├── PluginProcessor.*   ← audio DSP entry point
│   ├── PluginEditor.*      ← main GUI
│   ├── Saturator.h         ← multi-mode oversampled waveshaper
│   ├── Limiter.h           ← lookahead brick-wall limiter
│   ├── SpeedometerKnob.h   ← the gauge with pig mascot
│   ├── SegmentedModeSelector.h  ← pedal-style mode buttons
│   ├── WaveformDisplay.h   ← live input/output scope
│   ├── PeakHistory.h       ← lock-free ring buffer for scope
│   └── Theme.h             ← palette + fonts
└── Resources/              ← pig face PNGs (embedded into the plugin)
```

---

## Troubleshooting

- **"cmake: command not found"** — your terminal doesn't see CMake. On macOS run `brew install cmake`. On Windows reinstall CMake with *"Add CMake to system PATH"* enabled.
- **Plugin doesn't show up in DAW** — make sure your DAW is configured to scan VST3 plugins from the standard system folder. In Ableton: *Preferences → Plug-Ins → Use VST3 System Folders → ON*, then *Rescan*.
- **macOS won't open the plugin** — the build is ad-hoc signed (not notarized). If macOS blocks it, right-click → *Open* the first time, or `xattr -d com.apple.quarantine` on the `.vst3` bundle.
