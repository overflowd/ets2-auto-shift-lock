# ETS2 Auto Shift-Lock Plugin — Design Document

**Date:** 2026-11-24  
**Status:** Approved & Implemented

## 1. Intent & Motivation

In Euro Truck Simulator 2 (ETS2) **Real Automatic** (`automatic`) transmission mode, players can shift between `D`, `N`, and `R` without pressing the brake pedal and even while rolling. Real automatic vehicles use a shift-lock interlock that prevents shifting between `D`, `N`, and `R` unless the brake is applied and the vehicle is completely stopped.

This project provides a native Windows `x64` ETS2 plugin usable by everyone — compatible with **keyboards, gamepads, and all steering wheels / button boxes** — requiring no background application or virtual joystick drivers while driving.

## 2. Goals, Non-Goals, and Anti-Goals

### Goals
- Ship as a single drop-in native `x64` DLL (`ets2_auto_shift_lock.dll`) in ETS2's `bin/win_x64/plugins/` folder.
- **Smart mode routing:**
  - `Real Automatic` (`shifter.type == "automatic"`): Full brake + complete-stop shift lock active for both `Shift Up / Shift Down` and direct `D / N / R`.
  - `Sequential` (`shifter.type == "manual"`): Unlocked passthrough for `Shift Up` / `Shift Down` directly to `gearup` / `geardown` (so switching trucks never leaves shifting dead); direct `D / N / R` inactive.
  - `Simple Automatic` (`"arcade"`) and `H-Shifter` (`"hshifter"`): Completely inactive.
- Enforce **both** foot brake pressed AND vehicle completely stopped across all `D ↔ N ↔ R` transitions.
- Read brake input universally from SCS Telemetry SDK (`truck.input.brake` / `truck.effective.brake`) so every pedal, trigger, or key works identically.
- Poll shift inputs directly inside the plugin across Keyboard (Win32 VK), Gamepad (XInput incl. `LT`/`RT` triggers), and Steering Wheels / Button Boxes (DirectInput8).
- Support two momentary push-button shifting styles simultaneously:
  1. **Sequential:** `Shift Up` (`R → N → D`) and `Shift Down` (`D → N → R`).
  2. **Direct:** Dedicated momentary `D` (Drive), `N` (Neutral), and `R` (Reverse) buttons.
- Play an async audio warning sound (`lock_warning.wav`) whenever a shift attempt is blocked.
- Provide a lightweight standalone `.NET 9` configuration tool (`AutoShiftLockConfig.exe`) for setup, key/button binding, threshold tuning, and sound testing.

### Non-Goals
- Modifying ETS2's internal forward gear shifts (`D1`..`D12`) once in `D`.
- Clutch/gear lockouts for Manual, Sequential, or H-Shifter transmissions.
- Latched/held physical H-shifter gates or mechanical PRND levers in v1 (v1 supports momentary rising-edge push buttons only).
- In-game DirectX/OpenGL HUD overlays (avoids hooking instability and multiplayer/anti-cheat risk).
- Linux/macOS support in v1 (Windows `x64` ETS2 target; ATS works naturally via shared SCS SDK).

### Anti-Goals
- Requiring a background app or vJoy virtual driver while driving.
- Blocking valid shifts when stationary due to ETS2 physics jitter (`0.05 km/h` on hills/trailers).
- Dropping back to Neutral when releasing a momentary `D` or `R` button, or double-shifting.
- Crashing or stuttering ETS2 when a controller is unplugged or a WAV file is missing.
- Triggering shifts or warning audio while Alt-Tabbed out of ETS2 or paused.

## 3. Technical Constraints

1. **SCS Telemetry SDK is read-only:** Plugins cannot intercept or suppress keys bound directly in ETS2. Players clear ETS2's built-in `Shift Up` / `Shift Down` bindings and bind their physical controls in our plugin config; the plugin injects semantic commands into ETS2 via SCS Input SDK (`SCS_INPUT_DEVICE_TYPE_semantical`) only when lock conditions pass.
2. **ETS2 Real Automatic input behavior:** `gearup` and `geardown` are momentary pulses (`R ↔ N ↔ D`) and are wired into ETS2's default `controls.sii` as `semantical.gearup?0` and `semantical.geardown?0`, requiring zero manual virtual binding inside ETS2.
3. **Build toolchain:** Native C++17 `x64` DLL built via WinLibs MinGW-w64 UCRT (`-static`) + CMake + Ninja (zero VC++ or MinGW runtime DLL dependencies for end users); Configurator built with `.NET 9 SDK` as a self-contained single-file `win-x64` executable.

## 4. Approaches Considered & Selected

| Approach | How It Works | Trade-offs |
|---|---|---|
| **1. Native C++ ETS2 DLL + Lightweight `.NET` Configurator *(Selected)*** | Single `x64` DLL integrates SCS Telemetry + SCS Input SDK, polls KB/XInput/DirectInput8, plays WAV alerts. `.NET 9` app is only opened for setup/binding. | Requires C++ toolchain; one-time unbind of ETS2's built-in shift keys. |
| **2. Thin Bridge DLL + `.NET 9` Tray App** | Thin DLL bridges telemetry/input over shared memory; logic and UI run in a persistent tray app. | Requires a second `.exe` running in the background while driving; still requires a native DLL. |
| **3. Pure `.NET 9` + Existing Telemetry DLL + `SendInput`** | Reads telemetry from existing plugin and sends synthetic keys (`F13`/`F14`) to ETS2. | Requires manual virtual key binding in ETS2, background app, and fragile window-focus handling. |

**Selection:** Approach 1 delivers a true native ETS2 plugin with zero background apps and zero virtual drivers.

## 5. Architecture & Defaults

### Components
1. **`ets2_auto_shift_lock.dll` (C++17, x64):**
   - `config_store`: Shared INI schema, UTF-8 BOM/CRLF support, locale-invariant float parsing, value clamping, hot-reload `ConfigWatcher`.
   - `shift_lock_engine`: Pure unit-tested state machine routing `automatic`/`manual`/`arcade`/`hshifter`, preferring `displayed_gear` over raw `gear`, spacing two-step `D ↔ R` pulses (`pulse_ms`/`pulse_gap_ms`) with mid-transition lock re-check, and immediately clearing active pulses on pause or focus loss.
   - `audio_alert`: Async Win32 `PlaySoundW` with UTF-8 path support, cooldown, and `MessageBeep` fallback.
   - `input_poller`: Rising-edge universal poller for Keyboard (Win32 VK), XInput (`0..3`, incl. `LT`/`RT` at 50%), and DirectInput8 (`DIJOYSTATE2` buttons + POVs, deferred cooperative level, safe hot-unplug re-enumeration).
   - `scs_bridge` & `dll_main`: Official SCS SDK 1.14 Telemetry + Semantical Input (`gearup`, `geardown`) integration.
2. **`ets2_auto_shift_lock.ini`:** Human-readable shared config in `win_x64/plugins/`.
3. **`AutoShiftLockConfig.exe` (C# / `.NET 9` WinForms):** ETS2 Steam path auto-detection, one-click DLL+WAV install/update, Press-to-Bind capture across KB/XInput/DirectInput8, threshold/audio controls, and quick setup reminder.

### Default Decisions
- **Stop tolerance:** `0.50 km/h` (absorbs ETS2 physics jitter, configurable `0.0–10.0 km/h`).
- **Brake threshold & source:** `25%` (`0.25`) on `truck.input.brake` (foot brake pedal; parking brake does not unlock shifting).
- **Same-range press:** Silent no-op (no pulse, no warning beep).
- **Binding model (v1):** One binding per action (`shift_up`, `shift_down`, `gear_d`, `gear_n`, `gear_r`), including XInput `LT`/`RT` triggers.
- **Distribution:** Single ZIP containing self-contained `AutoShiftLockConfig.exe` + `plugin/ets2_auto_shift_lock.dll` + `plugin/lock_warning.wav` + `README.md`.
