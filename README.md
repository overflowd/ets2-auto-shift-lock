# ETS2 Auto Shift-Lock

A native Windows (`x64`) plugin for **Euro Truck Simulator 2 (ETS2)** that enforces realistic automatic transmission **shift-lock** behavior in **Real Automatic** mode — allowing transitions between **`D`, `N`, and `R` only while pressing the brake pedal and completely stopped**.

## Features

- **Universal hardware support:** Works with keyboards, XInput gamepads (including `LT`/`RT` triggers), and all DirectInput/HID steering wheels and button boxes. Brake detection reads directly from ETS2 telemetry, so every brake pedal/key/trigger works out of the box.
- **Zero background apps & zero virtual drivers:** Runs purely as a native SCS SDK DLL inside ETS2 (`ets2_auto_shift_lock.dll`). No vJoy and no tray app running while you drive.
- **Dual shifting styles:** Supports both sequential **Shift Up / Shift Down** (`R ↔ N ↔ D`) and dedicated momentary **Drive (`D`) / Neutral (`N`) / Reverse (`R`)** buttons simultaneously.
- **Smart transmission mode awareness:**
  - **Real Automatic (`automatic`):** Full brake + complete-stop shift lock active.
  - **Sequential (`manual`):** Unlocked passthrough for `Shift Up` / `Shift Down` so you never lose shifting when switching to a sequential truck.
  - **Simple Automatic (`arcade`) & H-Shifter (`hshifter`):** Automatically inactive.
- **Audio feedback:** Plays a subtle mechanical lock warning sound (`lock_warning.wav`, customizable) whenever a shift is blocked.
- **Live `.ini` hot-reload:** Changes saved from the configuration tool take effect without restarting ETS2.

## Installation (End Users)

1. Download and extract the release ZIP (`ets2-auto-shift-lock-v1.0.0-win-x64.zip`).
2. Run **`AutoShiftLockConfig.exe`**.
3. Your ETS2 `bin\win_x64\plugins` folder is auto-detected (or click **Browse...** to select it manually).
4. Click **Install / Update Plugin** to copy `ets2_auto_shift_lock.dll` and `lock_warning.wav`.
5. Bind your preferred shift controls (**Shift Up / Shift Down** and/or **Direct D / N / R**) using **Press to Bind**, then click **Save Settings**.
6. **One-time ETS2 in-game setup:**
   - Go to **Options → Controls → Transmission** and select **Real Automatic**.
   - Go to **Options → Keys & Buttons** and **unassign** ETS2's built-in **Shift Up** and **Shift Down** bindings (so the game does not bypass the brake lock).
7. Launch ETS2 and confirm the SDK plugin prompt. You can close `AutoShiftLockConfig.exe` once configured.

## Configuration (`ets2_auto_shift_lock.ini`)

Stored next to the DLL in `bin\win_x64\plugins\ets2_auto_shift_lock.ini`:

```ini
[thresholds]
brake_threshold=0.25
stop_speed_kmh=0.50
brake_source=input

[audio]
enabled=true
sound_file=lock_warning.wav
cooldown_ms=350

[timing]
pulse_ms=60
pulse_gap_ms=80

[bindings]
shift_up=kb:160
shift_down=kb:162
gear_d=
gear_n=
gear_r=
```

## Building from Source

Prerequisites: Windows x64, MinGW-w64 (UCRT x64) or MSVC, CMake, and `.NET 9 SDK`.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_release.ps1
```

This compiles `ets2_auto_shift_lock.dll`, runs all C++ (`27`) and `.NET` (`11`) unit tests, publishes a self-contained single-file `AutoShiftLockConfig.exe`, and packages everything into `dist/ets2-auto-shift-lock-v1.0.0-win-x64.zip`.

## License

MIT — see [LICENSE](LICENSE). Official SCS Software SDK 1.14 headers in `third_party/scs_sdk/` remain subject to [sdk_license.txt](third_party/scs_sdk/sdk_license.txt).
