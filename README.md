# PerformanieVJ

Cross-platform VJ software (Linux + Windows, macOS supported, iOS port planned) with a 64-bank cell grid, fullscreen video output, GPU-based effects / transitions / mixing, MIDI + keyboard control, Spout/Syphon texture input, and audio playback.

Native project format: `.pvj` (XML).
Import-only support for GrandVJ `.vj2` and Resolume `.avc`.

This repository is in early development. The plan is tracked in `.cursor/plans/` and implementation follows the milestones M1 - M12.

## Current status

Milestones **M1 (project setup)**, **M2 (data model + .pvj/.vj2/.avc serialization)**, **M3 (UI scaffold)** and **M4 (FFmpeg decode + thumbnails + preview playback)** are complete.

The application opens with a four-panel layout:

- right: collapsible **Media Library** dock (folder tree + project media list + preview thumbnail);
- top-left: **Parameter Inspector** tabs (Visual / Effect / Transition / Mixing / Position / Output) bound to the selected cell;
- top-right: two **Preview** placeholders (A / Preview, B / Output) - real GPU output arrives in M5;
- bottom: **Bank Grid** with 8x8 cells, bank tabs and an A / B bank-set toggle.

Double-clicking a media file in the dock assigns it to the currently selected cell and shows a thumbnail in the dock's preview. Triggering a cell (double-click in the grid) decodes the assigned clip through FFmpeg on a worker thread and renders it (looped) into the *A / Preview* pane. Project files use the native `.pvj` XML format; `.vj2` (GrandVJ) and `.avc` (Resolume) can be opened via *File - Import* (read-only).

## Prerequisites

- **CMake** >= 3.24
- **Ninja** (recommended generator)
- **C++20** compiler
  - Windows: MSVC 2022 (Visual Studio 17.8+) or clang-cl
  - Linux: GCC 12+ or Clang 15+
  - macOS: Xcode 15+
- **Qt 6.6+** - either installed via the official Qt installer or provided through vcpkg
- **FFmpeg 6.x** (avformat / avcodec / swscale / swresample) - either through vcpkg (see `vcpkg.json`) or installed system-wide (e.g. `apt install libavformat-dev libavcodec-dev libswscale-dev libswresample-dev` on Debian/Ubuntu, `brew install ffmpeg` on macOS, or a prebuilt dev package on Windows)

Optional:

- **vcpkg** with `VCPKG_ROOT` environment variable set (for reproducible Qt + later dependency builds)

## Building

### Option A - vcpkg manifest (recommended for reproducibility)

Set `VCPKG_ROOT` to your vcpkg checkout, then:

```bash
cmake --preset windows-debug    # or linux-debug / macos-debug
cmake --build --preset windows-debug
```

vcpkg will build `qtbase` on first configure (this takes a while; subsequent configures use the cache).

### Option B - System Qt

If Qt is installed via the official Qt installer, point CMake at it:

```powershell
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.7.3\msvc2022_64"
cmake --preset windows-qt-system
cmake --build build/windows-qt-system
```

On Linux:

```bash
cmake -B build -S . -DCMAKE_PREFIX_PATH=/opt/Qt/6.7.3/gcc_64 -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build
```

## Running

```bash
./build/<preset>/src/app/performanievj
```

You should see the *PerformanieVJ* window with the four-panel layout described above. Open the *File - Import* menu to load a `.vj2` or `.avc` project; opened projects can be saved as `.pvj`.

## Repository layout

```
CMakeLists.txt           # root project
CMakePresets.json        # configure + build presets per platform
vcpkg.json               # vcpkg manifest (dependencies)
src/
  app/                   # QApplication entry point + MainWindow
  core/                  # (M2) Project / Bank / Cell model + PvjSerializer + importers
  video/                 # (M4) FFmpeg MediaProbe + ThumbnailExtractor + VideoDecoder; (M6) DXV decoder
  render/                # (M5) QRhi-based render pipeline
  audio/                 # (M7) miniaudio-based audio engine
  input/                 # (M9) MIDI + keyboard input routing
  interop/               # (M10) Spout / Syphon / NDI sources
shaders/                 # (M5+) shader resources
tests/                   # (M2+) unit tests
```

## Feature flags

The root `CMakeLists.txt` exposes:

| Flag                  | Default             | Notes                              |
|-----------------------|---------------------|------------------------------------|
| `VJ_FEATURE_DXV`      | ON                  | Custom DXV codec decoder           |
| `VJ_FEATURE_SPOUT`    | ON (Windows only)   | Shared-texture input (DirectX 11)  |
| `VJ_FEATURE_SYPHON`   | ON (macOS only)     | Shared-texture input (Metal)       |
| `VJ_FEATURE_NDI`      | OFF                 | Cross-platform video-over-network  |
| `VJ_FEATURE_ASIO`     | OFF (Windows only)  | Low-latency audio on Windows       |
| `VJ_BUILD_TESTS`      | ON                  | Build unit tests                   |

## License

TBD.
