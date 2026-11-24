# Changelog

All notable changes to **ETS2 Auto Shift-Lock** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-11-25

### Added
- **Native Windows x64 ETS2 Plugin (`ets2_auto_shift_lock.dll`):**
  - Built directly against the official **SCS Telemetry & Input SDK 1.14** with zero background applications and zero virtual joystick drivers (no vJoy required).
  - Enforces real automatic transmission **shift-lock** in **Real Automatic** (`automatic`) mode: transitioning between `D`, `N`, and `R` is only permitted while pressing the brake (`>= 25%` default) and completely stopped (`<= 0.50 km/h` physics-jitter tolerance).
  - Supports both sequential **Shift Up / Shift Down** (`R ↔ N ↔ D`) and dedicated momentary **Drive (`D`) / Neutral (`N`) / Reverse (`R`)** buttons simultaneously.
  - Smart transmission mode routing: full shift-lock in **Real Automatic**, unlocked `Shift Up / Shift Down` passthrough in **Sequential** (`manual`), and automatic standby in **Simple Automatic** (`arcade`) and **H-Shifter** (`hshifter`).
  - Universal input capture across **Keyboard** (Win32 VK), **XInput Gamepads** (slots 0–3, including `LT`/`RT` triggers), and **DirectInput8 Steering Wheels & Button Boxes** (buttons + POV hats, with hot-unplug recovery).
  - Universal brake detection directly from ETS2 telemetry (`truck.input.brake` / `truck.effective.brake`).
  - Asynchronous mechanical shift-lock warning audio (`lock_warning.wav`) with configurable cooldown, custom WAV path (including UTF-8 paths), and `MessageBeep` fallback.
  - Automatic `.ini` hot-reload while ETS2 is running.
  - Safety handling for pause, Alt-Tab focus loss, mid-transition `D ↔ R` brake release aborts, and stationary `displayed_gear` vs. `engine_gear` telemetry alignment.
  - Statically linked against MinGW UCRT C++ runtime — no VC++ Redistributable or extra DLLs required.
- **Standalone `.NET 9` Configuration Tool (`AutoShiftLockConfig.exe`):**
  - Self-contained single-file Windows x64 executable (no `.NET Runtime` installation required).
  - Automatic ETS2 Steam installation detection (`libraryfolders.vdf` + drive scan) and one-click **Install / Update Plugin**.
  - Live **Press to Bind** capture for Keyboard, XInput, and DirectInput8 devices.
  - Configurable brake threshold, stop tolerance, brake source, and audio settings with live **Test Sound**.
  - **Dark & Light Mode** toggle with native Windows dark title bar and persistent theme preference.
  - Custom multi-resolution application icon (`16–256 px`).
- **Automated Testing & Packaging:**
  - `27` C++ unit tests (`plugin_tests`) and `13` `.NET` unit tests (`AutoShiftLockConfig.Tests`).
  - One-command release bundler (`scripts/build_release.ps1`) producing `ets2-auto-shift-lock-v1.0.0-win-x64.zip`.
