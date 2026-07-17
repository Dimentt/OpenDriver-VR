# OpenDriver-VR

OpenDriver-VR is a native SteamVR/OpenVR driver stack in modern C++ that consists of:
- A lightweight SteamVR-loaded driver (`driver_opendriver.dll` / `driver_opendriver.so`)
- A separate runtime process (`opendriver_runner.exe`)
- A multi-platform video encoding pipeline (Windows MF/NVENC, Linux x264)
- A bridge IPC (Named Pipes on Windows, Unix Sockets on Linux) between the runtime and the SteamVR-facing driver code
- A Qt dashboard UI for operations and configuration

The repository is laid out so it can be registered directly as a SteamVR driver after build.

## Architecture (High-level)

1. SteamVR loads `driver/driver.vrdrivermanifest`.
2. SteamVR loads `driver/bin/win64/driver_opendriver.dll`.
3. Driver launches `opendriver_runner.exe`.
4. Runtime initializes config and logging.
5. Virtual devices are registered and pose/input updates are pushed via the runtime.
6. Bridge IPC synchronizes runtime state with the driver-facing side.

## Features

### Core Runtime
- Native SteamVR/OpenVR driver integration.
- Runtime launched by driver to keep the driver process lightweight.
- Event bus with a thread-safe publish/subscribe model.
- Device registry for dynamic virtual devices.
- Centralized configuration management (JSON with dot-path access).

### Video Pipeline
- **Windows**: Media Foundation H.264 encoder path (DX11 -> NV12) and NVENC.
- **Linux**: x264 + swscale via DMA-BUF.
- Annex-B output handling.
- Encoder telemetry logging (attempts/failures/timing snapshots).
- Cooldown/backoff path after repeated encode failures.
- Zero-copy GPU fallback mechanisms.

### UI / Operations
- Qt dashboard (`opendriver_runner.exe`).
- Video settings panel.
- Install scripts for straightforward SteamVR registration.

## Windows Build

Requirements:
- Visual Studio 2022 with `Desktop development with C++`
- CMake 3.16+
- Qt6 for MSVC x64

Example configure/build:

```powershell
cmake -B build -A x64 -DCMAKE_BUILD_TYPE=Release -DQt6_DIR="C:\Qt\6.x\msvc2022_64\lib\cmake\Qt6"
cmake --build build --config Release --parallel
```

Build output is copied into `driver/bin/win64/`.

## SteamVR Installation

To build the release version and register the native SteamVR driver in one step:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1 -BuildRelease
```

---

## Changelog / Project State

### ✅ Implemented
- **Runtime**: Full lifecycle support (initialization, tick loop, shutdown) with precise tick rate pacing.
- **EventBus & DeviceRegistry**: Operational pub/sub system and VR device registration flow.
- **ConfigManager & Logger**: Working JSON configuration and spdlog wrapper with rolling buffer. Config paths correctly resolve to `APPDATA` on Windows and `~/.config` on Linux.
- **IPC**: Fully implemented with Named Pipes (Windows) and Unix Sockets (Linux).
- **SteamVR Driver**: Complete `driver_opendriver.cpp` containing `ITrackedDeviceServerDriver`, `IVRDisplayComponent`, and `IVRVirtualDisplay` implementations.
- **Windows Video Pipeline**: Highly optimized zero-copy D3D11 hardware encoding pipeline. Features fully implemented **Nvidia NVENC** encoding via nvEncodeAPI, with seamless fallback to **Media Foundation H.264**. Includes adaptive bitrate and telemetry logging.

### 🔴 Known Bugs & Critical Issues
- **Duplicate Properties**: Some SteamVR properties (e.g., `Prop_HmdTrackingStyle_Int32`, `Prop_DeviceBatteryPercentage_Float`) are being set multiple times incorrectly during device activation.

### 🟡 Work in Progress / To Do
- **Linux Video Pipeline**: The Linux `Present()` implementation is currently stubbed out entirely. Needs a software (e.g., x264 via DMA-BUF) or hardware encoding path to be written from scratch.
- **Input Bindings**: Verify the existence and formatting of `opendriver_hmd_profile.json` required by SteamVR.
- **Qt Resources**: Review `resources.qrc` as it may be referencing missing iconography.
