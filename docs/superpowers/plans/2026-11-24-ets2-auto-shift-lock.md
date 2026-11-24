# ETS2 Auto Shift-Lock Plugin Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a universal Windows x64 ETS2 plugin (`ets2_auto_shift_lock.dll`) that enforces real-automatic brake + complete-stop shift locking across `D`, `N`, and `R` for keyboard, gamepad, and steering-wheel players, paired with a standalone `.NET 9` setup/binding configurator (`AutoShiftLockConfig.exe`).

**Architecture:** A native C++17 x64 DLL loaded directly by ETS2 reads live telemetry (`truck.input.brake`, `truck.effective.brake`, `truck.speed`, `truck.gear`, `truck.displayed.gear`, `shifter.type`) via SCS Telemetry SDK, polls user-bound physical inputs across Keyboard (Win32 VK), Gamepad (XInput incl. `LT`/`RT`), and Steering Wheels/Button Boxes (DirectInput8), evaluates a pure unit-tested shift-lock state machine, and injects semantic `gearup`/`geardown` pulses via SCS Input SDK (`SCS_INPUT_DEVICE_TYPE_semantical`) or plays an async WAV warning sound on lock violation. A lightweight `.NET 9` WinForms tool shares the exact `.ini` and binding-token schema to detect ETS2, install/update plugin assets, capture bindings by pressing any button/key, and tune thresholds.

**Tech Stack:** C++17, CMake, Win32 API (`user32`, `dinput8`, `dxguid`, `xinput`, `winmm`), Official SCS SDK 1.14 headers, Python 3 (deterministic WAV generation), C# / `.NET 9` (WinForms + xUnit tests), PowerShell release bundler.

**Spec:** [docs/superpowers/specs/2026-11-24-ets2-auto-shift-lock-design.md](../../specs/2026-11-24-ets2-auto-shift-lock-design.md)

## Global Constraints

- Platform: Windows x64 (`ets2_auto_shift_lock.dll` loaded by ETS2 `win_x64/plugins/`; ATS compatible as natural byproduct).
- Zero background apps and zero third-party virtual drivers (no vJoy) while driving.
- Active transmission routing:
  - `shifter.type == "automatic"` (Real Automatic): full brake + complete-stop shift lock for `shift_up`, `shift_down`, `gear_d`, `gear_n`, `gear_r`.
  - `shifter.type == "manual"` (Sequential): unlocked passthrough for `shift_up` → `gearup` and `shift_down` → `geardown`; `gear_d`/`gear_n`/`gear_r` ignored, no warning audio.
  - `shifter.type == "arcade"` or `"hshifter"` (or unknown/empty): completely inactive.
- All 5 actions are rising-edge momentary triggers only (1 binding slot per action in v1; no latched H-shifter gates, no multi-binding lists or modifier chords).
- Default thresholds: `brake_threshold = 0.25`, `stop_speed_kmh = 0.5`, `brake_source = input`, `audio.enabled = true`, `audio.sound_file = lock_warning.wav`, `audio.cooldown_ms = 350`, `timing.pulse_ms = 60`, `timing.pulse_gap_ms = 80`.
- Shared binding token format between C++ DLL and C# Configurator:
  - Empty string: unbound
  - Keyboard: `kb:<vk_decimal>` (e.g., `kb:160`)
  - XInput: `xinput:<slot_0_to_3>:<A|B|X|Y|LB|RB|BACK|START|LS|RS|DPAD_UP|DPAD_DOWN|DPAD_LEFT|DPAD_RIGHT|LT|RT>`
  - DirectInput button: `dinput:<PRODUCT_GUID_UPPERCASE_HYPHENATED>:btn:<0_based_index>`
  - DirectInput POV: `dinput:<PRODUCT_GUID_UPPERCASE_HYPHENATED>:pov:<0_based_index>:<UP|RIGHT|DOWN|LEFT>`
- Release distribution: single ZIP containing self-contained `AutoShiftLockConfig.exe` + `plugin/ets2_auto_shift_lock.dll` + `plugin/lock_warning.wav`.
- Configurator UI strings in Turkish matching user language.

## Review Focus

1. **Mid-transition abort on two-step `D ↔ R` shift:** If the player releases the brake or the truck starts moving between pulse 1 (`D → N`) and pulse 2 (`N → R`), the pending second pulse must cancel cleanly and release any active output signal rather than completing an unlocked shift or sticking `geardown` on `true`. (Pinned in Task 3.)
2. **Stationary `D` telemetry divergence (`gear` vs `displayed_gear`):** When stopped in `D`, ETS2 may report `gear == 0` while `displayed_gear > 0` (or vice versa). Range derivation must prefer `displayed_gear` when non-zero and fall back to `gear`, preventing false `N` detection or spurious warning beeps. (Pinned in Task 3.)
3. **Focus loss / pause boundary:** When ETS2 loses foreground focus or fires `paused == true`, any in-flight shift pulse must drop to `false` immediately and input edge trackers must reset so unpausing/Alt-Tabbing back never emits a ghost shift or beep. (Pinned in Tasks 3 & 5.)
4. **Corrupt / BOM / out-of-range `ets2_auto_shift_lock.ini`:** Hand-edited files with UTF-8 BOM, CRLF/LF mix, negative speed, `brake_threshold > 1.0`, or unknown binding syntax must clamp numeric settings safely (`brake_threshold` ∈ `[0.01, 1.0]`, `stop_speed_kmh` ∈ `[0.0, 10.0]`, `cooldown_ms` ∈ `[0, 5000]`), ignore invalid bindings, and never crash. (Pinned in Tasks 2 & 7.)
5. **Unplugged DirectInput device & missing WAV file:** Unplugging a steering wheel mid-drive (`DIERR_INPUTLOST` / `DIERR_NOTACQUIRED`) must recover safely via re-acquire / throttled re-enumeration without stalling the ETS2 frame loop; missing `lock_warning.wav` must fall back to `MessageBeep` without failing. (Pinned in Tasks 4 & 5.)

---

### Task 1: Repository Setup, C++ Toolchain & Official SCS SDK 1.14 Headers

**Files:**
- Create: `.gitignore`
- Create: `CMakeLists.txt`
- Create: `third_party/scs_sdk/README.md` (records SDK version & source URL) + official SCS SDK 1.14 headers under `third_party/scs_sdk/include/`
- Create: `tests/plugin/test_main.cpp`
- Create: `tests/plugin/test_framework.hpp`

**Interfaces:**
- Produces:
  - Git repository initialized on `main`
  - Verified x64 C++17 compiler + CMake build pipeline (`cmake -B build ...` & `ctest --test-dir build --output-on-failure`)
  - Official SCS SDK 1.14 headers verified on disk (`scssdk.h`, `scssdk_input.h`, `scssdk_telemetry.h`, `common/scssdk_telemetry_truck_common_channels.h`, `common/scssdk_telemetry_truck_common_configs.h`, `eurotrucks2/scssdk_eut2.h`, `eurotrucks2/scssdk_input_eut2.h`)
  - Zero-dependency test macro header `tests/plugin/test_framework.hpp` (`TEST_CASE(name)`, `CHECK(expr)`, `CHECK_EQ(a, b)`, `CHECK_NEAR(a, b, eps)`)

- [ ] **Step 1: Initialize git repo and verify/install x64 C++17 toolchain + CMake**

Check for existing `cl.exe` / `g++` / `clang++` / `cmake`. If absent, install MinGW-w64 (UCRT x64) + CMake via `winget` (or Visual Studio Build Tools if user prefers) and verify `cmake --version` and x64 compiler target. Initialize git (`git init -b main`) and create `.gitignore` ignoring `build/`, `dist/`, `**/bin/`, `**/obj/`, `.vs/`, `*.user`.

- [ ] **Step 2: Fetch and vendor official SCS SDK 1.14 headers**

Download the official SCS SDK 1.14 archive from SCS Software (`modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry`), extract `include/` into `third_party/scs_sdk/include/`, and inspect `scssdk_input.h` + `eurotrucks2/scssdk_input_eut2.h` to verify exact struct names, API version macro (`SCS_INPUT_VERSION_1_00`), and exact semantic input identifiers (`gearup`, `geardown`, etc.). Record findings in `third_party/scs_sdk/README.md`.

- [ ] **Step 3: Create minimal test harness and baseline `CMakeLists.txt`**

Create `tests/plugin/test_framework.hpp` (registry vector of named tests, `CHECK` / `CHECK_EQ` / `CHECK_NEAR` macros printing file/line on failure) and `tests/plugin/test_main.cpp` running all registered tests and returning `0` on pass / `1` on failure. Create `CMakeLists.txt` enforcing C++17, x64 pointer size check (`CMAKE_SIZEOF_VOID_P EQUAL 8`), `enable_testing()`, and target `plugin_tests`.

- [ ] **Step 4: Build and run baseline test executable**

Run: `cmake -B build && cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
Expected: `100% tests passed` (0 failures, 1 smoke test passing).

- [ ] **Step 5: Commit**

```bash
git add .gitignore CMakeLists.txt third_party/scs_sdk tests/plugin/test_framework.hpp tests/plugin/test_main.cpp docs/superpowers/
git commit -m "chore: scaffold C++17 CMake project, test harness, and SCS SDK 1.14 headers"
```

---

### Task 2: Binding Schema & INI Configuration Store (`config_store`)

**Files:**
- Create: `plugin/include/binding_types.hpp`
- Create: `plugin/include/config_store.hpp`
- Create: `plugin/src/config_store.cpp`
- Create: `tests/plugin/test_config_store.cpp`
- Modify: `CMakeLists.txt` (add `config_store.cpp` and `test_config_store.cpp`)

**Interfaces:**
- Consumes: `tests/plugin/test_framework.hpp`
- Produces:
  - `enum class ShiftAction : uint8_t { ShiftUp, ShiftDown, GearD, GearN, GearR, Count };`
  - `enum class BindingSource : uint8_t { None, Keyboard, XInput, DirectInputButton, DirectInputPov };`
  - `enum class PovDirection : uint8_t { Up, Right, Down, Left };`
  - `enum class BrakeSource : uint8_t { Input, Effective };`
  - `struct BindingSpec { BindingSource source = BindingSource::None; uint32_t vk_code = 0; uint8_t xinput_slot = 0; std::string xinput_button; std::string device_guid; uint32_t index = 0; PovDirection pov_dir = PovDirection::Up; };`
  - `struct PluginConfig` containing:
    - `float brake_threshold = 0.25f;`
    - `float stop_speed_kmh = 0.5f;`
    - `BrakeSource brake_source = BrakeSource::Input;`
    - `bool audio_enabled = true;`
    - `std::string audio_sound_file = "lock_warning.wav";`
    - `uint32_t audio_cooldown_ms = 350;`
    - `uint32_t pulse_ms = 60;`
    - `uint32_t pulse_gap_ms = 80;`
    - `std::array<BindingSpec, 5> bindings;`
  - `BindingSpec parse_binding_token(std::string_view token);`
  - `std::string format_binding_token(const BindingSpec& spec);`
  - `PluginConfig parse_ini_text(std::string_view text);`
  - `std::string serialize_ini_text(const PluginConfig& cfg);`
  - `class ConfigWatcher { public: explicit ConfigWatcher(std::filesystem::path ini_path); bool reload_if_changed(PluginConfig& out_cfg); };`

- [ ] **Step 1: Write failing unit tests in `tests/plugin/test_config_store.cpp`**

```cpp
TEST_CASE(config_defaults_match_spec) {
    auto cfg = parse_ini_text("");
    CHECK_NEAR(cfg.brake_threshold, 0.25f, 1e-4f);
    CHECK_NEAR(cfg.stop_speed_kmh, 0.5f, 1e-4f);
    CHECK(cfg.brake_source == BrakeSource::Input);
    CHECK(cfg.audio_enabled == true);
    CHECK_EQ(cfg.audio_sound_file, std::string("lock_warning.wav"));
    CHECK_EQ(cfg.audio_cooldown_ms, 350u);
    CHECK_EQ(cfg.pulse_ms, 60u);
    CHECK_EQ(cfg.pulse_gap_ms, 80u);
}

TEST_CASE(binding_token_round_trip_all_sources) {
    for (std::string tok : {
            "kb:160",
            "xinput:0:RB",
            "xinput:2:LT",
            "dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:btn:4",
            "dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:pov:0:UP"}) {
        auto parsed = parse_binding_token(tok);
        CHECK(parsed.source != BindingSource::None);
        CHECK_EQ(format_binding_token(parsed), tok);
    }
    CHECK(parse_binding_token("").source == BindingSource::None);
    CHECK(parse_binding_token("dinput:bad:xyz").source == BindingSource::None);
}

TEST_CASE(config_handles_bom_crlf_and_clamps_invalid_values) {
    std::string raw = "\xEF\xBB\xBF[thresholds]\r\n"
                      "brake_threshold = 2.5\r\n"
                      "stop_speed_kmh = -3.0\r\n"
                      "brake_source = effective\r\n"
                      "[audio]\r\n"
                      "cooldown_ms = 99999\r\n"
                      "[bindings]\r\n"
                      "shift_up = kb:160\r\n"
                      "gear_d = garbage_token\r\n";
    auto cfg = parse_ini_text(raw);
    CHECK_NEAR(cfg.brake_threshold, 1.0f, 1e-4f);
    CHECK_NEAR(cfg.stop_speed_kmh, 0.0f, 1e-4f);
    CHECK(cfg.brake_source == BrakeSource::Effective);
    CHECK_EQ(cfg.audio_cooldown_ms, 5000u);
    CHECK(cfg.bindings[static_cast<size_t>(ShiftAction::ShiftUp)].source == BindingSource::Keyboard);
    CHECK(cfg.bindings[static_cast<size_t>(ShiftAction::GearD)].source == BindingSource::None);
}

TEST_CASE(config_watcher_reloads_on_mtime_change) {
    // Write temp ini, verify first reload returns true with initial binding,
    // second call returns false, modify file + bump mtime, third call returns true with new binding.
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
Expected: FAIL (compile error — headers/functions not yet implemented).

- [ ] **Step 3: Implement `binding_types.hpp`, `config_store.hpp`, and `config_store.cpp`**

Implement case-insensitive section/key parsing, UTF-8 BOM stripping, whitespace trimming, comment (`;`, `#`) skipping, clamping (`brake_threshold` ∈ `[0.01f, 1.0f]`, `stop_speed_kmh` ∈ `[0.0f, 10.0f]`, `audio_cooldown_ms` ∈ `[0, 5000]`, `pulse_ms` ∈ `[20, 500]`, `pulse_gap_ms` ∈ `[20, 500]`), uppercase GUID normalization, and `ConfigWatcher` using `std::filesystem::last_write_time` with `std::error_code` (never throws if file missing).

- [ ] **Step 4: Build and run tests to verify they pass**

Run: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
Expected: PASS (all config_store tests green).

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt plugin/include/binding_types.hpp plugin/include/config_store.hpp plugin/src/config_store.cpp tests/plugin/test_config_store.cpp
git commit -m "feat(plugin): add binding schema and hot-reloading INI config store"
```

---

### Task 3: Pure C++ Shift-Lock State Machine (`shift_lock_engine`)

**Files:**
- Create: `plugin/include/shift_lock_engine.hpp`
- Create: `plugin/src/shift_lock_engine.cpp`
- Create: `tests/plugin/test_shift_lock_engine.cpp`
- Modify: `CMakeLists.txt` (add `shift_lock_engine.cpp` and `test_shift_lock_engine.cpp`)

**Interfaces:**
- Consumes: `plugin/include/binding_types.hpp`, `plugin/include/config_store.hpp`
- Produces:
  - `enum class ShifterMode : uint8_t { Unknown, Arcade, Automatic, Manual, HShifter };`
  - `enum class GearRange : int8_t { Reverse = -1, Neutral = 0, Drive = 1 };`
  - `struct TelemetrySnapshot { ShifterMode mode = ShifterMode::Unknown; bool paused = true; float input_brake = 0.0f; float effective_brake = 0.0f; float speed_mps = 0.0f; int32_t gear = 0; int32_t displayed_gear = 0; };`
  - `struct EngineOutput { bool gearup_active = false; bool geardown_active = false; bool trigger_lock_audio = false; };`
  - `ShifterMode parse_shifter_mode(std::string_view scs_shifter_type);`
  - `GearRange derive_gear_range(int32_t gear, int32_t displayed_gear);`
  - `class ShiftLockEngine { public: void on_action_pressed(ShiftAction action, const TelemetrySnapshot& t, const PluginConfig& cfg, uint64_t now_ms); EngineOutput tick(const TelemetrySnapshot& t, const PluginConfig& cfg, uint64_t now_ms); void reset(); };`

- [ ] **Step 1: Write failing unit tests in `tests/plugin/test_shift_lock_engine.cpp`**

Pin every core rule and Review Focus items #1, #2, #3:
```cpp
TEST_CASE(automatic_blocks_all_dnr_transitions_without_brake_and_fires_audio) {
    // Test N->D, D->N, N->R, R->N, D->R, R->D with speed=0, input_brake=0.10 (< 0.25):
    // gearup_active==false, geardown_active==false, trigger_lock_audio==true.
}

TEST_CASE(automatic_blocks_shift_when_moving_above_tolerance_even_with_brake) {
    // speed_mps = 0.30f (1.08 km/h > 0.5 km/h), input_brake = 0.8f -> blocked + audio.
    // speed_mps = 0.10f (0.36 km/h <= 0.5 km/h), input_brake = 0.3f -> allowed, gearup_active==true, no audio.
}

TEST_CASE(same_range_press_is_silent_noop) {
    // Already in D (displayed_gear=1), press GearD or ShiftUp with brake=0, speed=20 m/s -> no pulse, no audio.
    // Already in R (displayed_gear=-1), press GearR or ShiftDown -> no pulse, no audio.
    // Already in N (displayed_gear=0, gear=0), press GearN -> no pulse, no audio.
}

TEST_CASE(direct_d_to_r_emits_two_spaced_geardown_pulses) {
    // In D, brake=0.8, speed=0 -> press GearR at t=0:
    // t=0..59: geardown_active==true; t=60..139: both false; t=140..199: geardown_active==true; t=200: both false.
}

TEST_CASE(mid_transition_d_to_r_aborts_second_pulse_if_brake_released_or_moving) {
    // Review Focus #1: Start D->R at t=0. At t=70 (during gap), set input_brake=0.0f.
    // At t=140: geardown_active MUST stay false (aborted, never completes unlocked 2nd shift).
}

TEST_CASE(stationary_d_displayed_gear_beats_zero_raw_gear) {
    // Review Focus #2: gear=0, displayed_gear=1 -> derive_gear_range == GearRange::Drive.
    // Pressing GearD produces silent no-op; pressing GearN with brake+stop emits 1 geardown pulse.
}

TEST_CASE(manual_mode_passes_shift_up_down_without_lock_and_ignores_dnr) {
    // mode=ShifterMode::Manual, brake=0, speed=25 m/s:
    // ShiftUp -> gearup_active==true, no audio; ShiftDown -> geardown_active==true, no audio;
    // GearD/N/R -> no pulse, no audio.
}

TEST_CASE(arcade_and_hshifter_modes_are_completely_inactive) {
    // mode=Arcade and HShifter -> all 5 actions produce no pulse and no audio.
}

TEST_CASE(pause_immediately_clears_active_pulse_and_pending_queue) {
    // Review Focus #3: Start shift pulse at t=0, set paused=true at t=10 -> tick() returns both false and stays false after unpause.
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
Expected: FAIL (headers/symbols missing).

- [ ] **Step 3: Implement `shift_lock_engine.hpp` and `shift_lock_engine.cpp`**

Implement `parse_shifter_mode` (`"arcade"`, `"automatic"`, `"manual"`, `"hshifter"`), `derive_gear_range` (check `displayed_gear != 0 ? sign(displayed_gear) : sign(gear)`), and `ShiftLockEngine`:
- Internal small pulse queue (capacity 2 pulses: `{bool is_up, bool requires_lock}`) with explicit state (`Idle`, `Pulsing`, `Gap`) timed against `now_ms`, `pulse_ms`, `pulse_gap_ms`.
- On `paused == true` or `mode == Arcade/HShifter/Unknown`: call `reset()` immediately so no output sticks `true`.
- When advancing from `Gap` to next pulse where `requires_lock == true`: re-evaluate `brake >= brake_threshold && |speed_mps| * 3.6f <= stop_speed_kmh`; if violated, clear queue cleanly.
- `trigger_lock_audio` is a one-shot edge flag returned by the next `tick()` and cleared immediately after returning.

- [ ] **Step 4: Build and run tests to verify they pass**

Run: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
Expected: PASS (all shift-lock engine tests green).

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt plugin/include/shift_lock_engine.hpp plugin/src/shift_lock_engine.cpp tests/plugin/test_shift_lock_engine.cpp
git commit -m "feat(plugin): implement shift-lock state machine with full unit test coverage"
```

---

### Task 4: Audio Alert Module & Default Mechanical Lock WAV

**Files:**
- Create: `assets/generate_lock_wav.py`
- Create: `assets/lock_warning.wav` (generated by script)
- Create: `plugin/include/audio_alert.hpp`
- Create: `plugin/src/audio_alert.cpp`
- Create: `tests/plugin/test_audio_alert.cpp`
- Modify: `CMakeLists.txt` (add `audio_alert.cpp`, link `winmm`, add `test_audio_alert.cpp`)

**Interfaces:**
- Consumes: `plugin/include/config_store.hpp`
- Produces:
  - `enum class AlertBackendResult : uint8_t { SkippedDisabled, SkippedCooldown, PlayedWav, PlayedFallbackBeep };`
  - `class AudioAlert { public: explicit AudioAlert(std::filesystem::path plugin_dir, bool dry_run = false); AlertBackendResult try_play(const PluginConfig& cfg, uint64_t now_ms); void reset_cooldown(); };`

- [ ] **Step 1: Create `assets/generate_lock_wav.py` and generate `assets/lock_warning.wav`**

Write a small stdlib-only Python script (`wave`, `math`, `struct`) that synthesizes a crisp, subtle 180 ms 16-bit 44.1 kHz mono mechanical shift-lock double-click / low muted tone (non-jarring, realistic automotive shift-lock feel) and writes `assets/lock_warning.wav`. Run `python assets/generate_lock_wav.py` and verify non-empty valid RIFF/WAVE file.

- [ ] **Step 2: Write failing tests in `tests/plugin/test_audio_alert.cpp`**

```cpp
TEST_CASE(audio_alert_respects_enabled_flag_and_cooldown) {
    // dry_run=true so tests never play real speakers on CI/dev machines.
    // Disabled -> SkippedDisabled.
    // Enabled with existing assets/lock_warning.wav at t=1000 -> PlayedWav.
    // Again at t=1200 (< 350ms cooldown) -> SkippedCooldown.
    // Again at t=1400 (>= 350ms cooldown) -> PlayedWav.
}

TEST_CASE(audio_alert_falls_back_when_wav_missing) {
    // Review Focus #5: cfg.audio_sound_file = "nonexistent.wav" -> PlayedFallbackBeep (never throws).
}
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
Expected: FAIL (headers/symbols missing).

- [ ] **Step 4: Implement `audio_alert.hpp` and `audio_alert.cpp`**

Resolve relative `cfg.audio_sound_file` against `plugin_dir` (absolute paths supported as-is). Enforce `now_ms - last_play_ms >= cfg.audio_cooldown_ms`. When `dry_run == false`: if file exists call `PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)` (falling back to `MessageBeep(MB_ICONWARNING)` if `PlaySoundW` returns `FALSE` or file missing).

- [ ] **Step 5: Build and run tests to verify they pass**

Run: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt assets/generate_lock_wav.py assets/lock_warning.wav plugin/include/audio_alert.hpp plugin/src/audio_alert.cpp tests/plugin/test_audio_alert.cpp
git commit -m "feat(plugin): add async WAV audio alert with cooldown and fallback"
```

---

### Task 5: Universal Input Poller (Keyboard + XInput + DirectInput8)

**Files:**
- Create: `plugin/include/input_poller.hpp`
- Create: `plugin/src/input_poller.cpp`
- Create: `tests/plugin/test_input_poller.cpp`
- Modify: `CMakeLists.txt` (add `input_poller.cpp`, link `user32`, `dinput8`, `dxguid`, `xinput`, add `test_input_poller.cpp`)

**Interfaces:**
- Consumes: `plugin/include/binding_types.hpp`, `plugin/include/config_store.hpp`
- Produces:
  - `class InputPoller { public: InputPoller(); ~InputPoller(); void initialize(HINSTANCE module_instance); void shutdown(); void reset_edges(); std::array<bool, 5> poll_pressed_edges(const PluginConfig& cfg, bool allow_capture, uint64_t now_ms); static bool eval_pov_direction(uint32_t raw_pov_hundredths, PovDirection dir); static bool eval_xinput_button(const XINPUT_GAMEPAD& gp, std::string_view button_name); };`

- [ ] **Step 1: Write failing unit tests in `tests/plugin/test_input_poller.cpp`**

```cpp
TEST_CASE(pov_direction_matching_handles_center_and_diagonals) {
    // 0xFFFF / 0xFFFFFFFF (centered) -> all false.
    // 0 (0 deg) -> Up true, others false.
    // 9000 (90 deg) -> Right true.
    // 18000 (180 deg) -> Down true.
    // 27000 (270 deg) -> Left true.
    // 4500 (NE) -> Up true and Right true.
}

TEST_CASE(xinput_button_eval_supports_digital_and_analog_triggers) {
    // RB bitmask -> true; LT=120 (< 128) -> false; LT=140 (>= 128) -> true; RT=200 -> true.
}

TEST_CASE(allow_capture_false_resets_edges_without_firing) {
    // Review Focus #3: calling poll_pressed_edges(..., allow_capture=false, ...) returns all false and resets edge state.
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
Expected: FAIL (headers/symbols missing).

- [ ] **Step 3: Implement `input_poller.hpp` and `input_poller.cpp`**

Implement:
- Foreground window guard helper (`is_current_process_foreground()` using `GetForegroundWindow` + `GetWindowThreadProcessId` == `GetCurrentProcessId`).
- Keyboard polling via `GetAsyncKeyState(vk) & 0x8000` (only when `allow_capture && is_current_process_foreground()`).
- XInput polling via `XInputGetState(slot, &state)` with `LT`/`RT` threshold `>= 128` (`50%`).
- DirectInput8 initialization (`DirectInput8Create` with `DISCL_BACKGROUND | DISCL_NONEXCLUSIVE` so it never conflicts with ETS2's own DirectInput steering/pedal acquisition!), enumerating attached `DI8DEVCLASS_GAMECTRL` devices, formatting `DIDEVICEINSTANCEW.guidProduct` as uppercase hyphenated `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`, reading `DIJOYSTATE2` (`rgbButtons[0..127]` and `rgdwPOV[0..3]`), re-acquiring on `DIERR_INPUTLOST` / `DIERR_NOTACQUIRED`, and throttling device re-enumeration to at most once every `3000 ms` when a bound GUID is currently missing (Review Focus #5).
- Per-action `prev_down[5]` rising-edge tracker cleared whenever `!allow_capture` or `!is_current_process_foreground()` (Review Focus #3).

- [ ] **Step 4: Build and run tests to verify they pass**

Run: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt plugin/include/input_poller.hpp plugin/src/input_poller.cpp tests/plugin/test_input_poller.cpp
git commit -m "feat(plugin): add universal Keyboard, XInput, and DirectInput8 edge poller"
```

---

### Task 6: SCS Telemetry & Input SDK Bridge + Native x64 DLL (`ets2_auto_shift_lock.dll`)

**Files:**
- Create: `plugin/include/scs_bridge.hpp`
- Create: `plugin/src/scs_bridge.cpp`
- Create: `plugin/src/dll_main.cpp`
- Create: `plugin/exports.def` (ensures undecorated x64 exports: `scs_telemetry_init`, `scs_telemetry_shutdown`, `scs_input_init`, `scs_input_shutdown`)
- Modify: `CMakeLists.txt` (add `SHARED` library target `ets2_auto_shift_lock` linking `user32`, `dinput8`, `dxguid`, `xinput`, `winmm`)

**Interfaces:**
- Consumes: `third_party/scs_sdk/include/*`, `config_store.hpp`, `shift_lock_engine.hpp`, `audio_alert.hpp`, `input_poller.hpp`
- Produces:
  - `build/Release/ets2_auto_shift_lock.dll` (or `build/ets2_auto_shift_lock.dll` on MinGW) exporting exact SCS entry points:
    - `SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t* const params)`
    - `SCSAPI_VOID scs_telemetry_shutdown(void)`
    - `SCSAPI_RESULT scs_input_init(const scs_u32_t version, const scs_input_init_params_t* const params)`
    - `SCSAPI_VOID scs_input_shutdown(void)`

- [ ] **Step 1: Implement `scs_bridge.hpp`, `scs_bridge.cpp`, `dll_main.cpp`, and `plugin/exports.def`**

Wire together:
- `DllMain` captures `HMODULE` to resolve plugin directory (`GetModuleFileNameW`) for `ets2_auto_shift_lock.ini` and `lock_warning.wav`.
- `scs_telemetry_init`:
  - Checks `version == SCS_TELEMETRY_VERSION_1_01` (or SDK 1.14's current version verified in Task 1).
  - Registers config callback (`SCS_TELEMETRY_EVENT_configuration`) extracting `shifter.type` (`SCS_TELEMETRY_CONFIG_ATTRIBUTE_shifter_type`) when config id is `"controls"` (verified in Task 1 headers!).
  - Registers lifecycle callbacks (`paused`, `started`, `frame_end`).
  - Registers channels: `truck.input.brake` (`float`), `truck.effective.brake` (`float`), `truck.speed` (`float`), `truck.gear` (`s32`), `truck.displayed.gear` (`s32`).
  - Initializes `ConfigWatcher`, `AudioAlert`, `InputPoller`.
- `scs_input_init`:
  - Registers a single `SCS_INPUT_DEVICE_TYPE_semantical` device (id `"auto_shift_lock"`, display name `"Auto Shift Lock"`) with 2 boolean inputs (`"gearup"`, `"geardown"` — exact names verified against Task 1 SDK headers!) and an `input_event_callback` that emits state transitions whenever `gearup_active` or `geardown_active` changes (using `std::mutex` / atomics cleanly between `frame_end` and `input_event_callback`).
- On `frame_end`:
  - Throttled (~500 ms) `ConfigWatcher::reload_if_changed`.
  - Polls `InputPoller::poll_pressed_edges(cfg, !snapshot.paused, now_ms)`.
  - Feeds pressed actions into `ShiftLockEngine::on_action_pressed`, runs `ShiftLockEngine::tick`, publishes boolean edge updates to the SCS Input event queue, and invokes `AudioAlert::try_play` if `trigger_lock_audio == true`.
- On `scs_telemetry_shutdown` / `scs_input_shutdown`: cleanly resets engine, shuts down `InputPoller`, logs clean shutdown via `scs_log`.

- [ ] **Step 2: Build the x64 DLL and verify exported symbols**

Run: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
Verify DLL exports using `objdump -p` (MinGW) or `dumpbin /exports` (MSVC) or a short Python PE export parser: confirm `ets2_auto_shift_lock.dll` is PE32+ (x64) and exports `scs_telemetry_init`, `scs_telemetry_shutdown`, `scs_input_init`, `scs_input_shutdown`.

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt plugin/exports.def plugin/include/scs_bridge.hpp plugin/src/scs_bridge.cpp plugin/src/dll_main.cpp
git commit -m "feat(plugin): wire SCS Telemetry/Input SDK bridge and build x64 plugin DLL"
```

---

### Task 7: `.NET 9` Configurator App (`AutoShiftLockConfig.exe`), Release Script & Docs

**Files:**
- Create: `configurator/AutoShiftLockConfig.sln`
- Create: `configurator/src/AutoShiftLockConfig/AutoShiftLockConfig.csproj` (`net9.0-windows`, `UseWindowsForms=true`, `PlatformTarget=x64`)
- Create: `configurator/src/AutoShiftLockConfig/Models/BindingSpec.cs`
- Create: `configurator/src/AutoShiftLockConfig/Models/PluginConfig.cs`
- Create: `configurator/src/AutoShiftLockConfig/Services/Ets2PathDetector.cs`
- Create: `configurator/src/AutoShiftLockConfig/Services/UniversalInputCapture.cs`
- Create: `configurator/src/AutoShiftLockConfig/MainForm.cs`
- Create: `configurator/src/AutoShiftLockConfig/Program.cs`
- Create: `configurator/tests/AutoShiftLockConfig.Tests/AutoShiftLockConfig.Tests.csproj`
- Create: `configurator/tests/AutoShiftLockConfig.Tests/PluginConfigTests.cs`
- Create: `configurator/tests/AutoShiftLockConfig.Tests/Ets2PathDetectorTests.cs`
- Create: `scripts/build_release.ps1`
- Create: `README.md`

**Interfaces:**
- Consumes: Shared `.ini` schema & binding token format from Task 2; built `ets2_auto_shift_lock.dll` + `assets/lock_warning.wav` from Tasks 4 & 6.
- Produces:
  - `BindingSpec.Parse(string token)`, `BindingSpec.ToToken()`, `BindingSpec.ToDisplayText()`
  - `PluginConfig.Parse(string iniText)`, `PluginConfig.Serialize()`, `PluginConfig.Load(string path)`, `PluginConfig.Save(string path)`
  - `Ets2PathDetector.FindEts2PluginsDirectories(IEnumerable<string>? candidateRoots = null)`, `Ets2PathDetector.InstallPluginFiles(string sourcePluginDir, string targetPluginsDir)` (copies `ets2_auto_shift_lock.dll` and `lock_warning.wav` without overwriting an existing user `ets2_auto_shift_lock.ini`)
  - `UniversalInputCapture` (captures next rising edge across Keyboard VK, XInput slots 0–3 incl. `LT`/`RT`, and DirectInput8 buttons/POVs via P/Invoke/COM matching C++ `DIDEVICEINSTANCEW.guidProduct` uppercase hyphenated GUIDs 1:1)
  - `scripts/build_release.ps1` generating `dist/ets2-auto-shift-lock-v1.0.0-win-x64.zip`

- [ ] **Step 1: Scaffold `.NET 9` solution & write failing unit tests (`PluginConfigTests.cs`, `Ets2PathDetectorTests.cs`)**

Create `.sln`, app `.csproj`, and xUnit test `.csproj`. Write failing tests covering:
- All 5 binding token types round-trip identically to C++ format (`kb:160`, `xinput:0:RB`, `xinput:1:RT`, `dinput:{UPPER-GUID}:btn:4`, `dinput:{UPPER-GUID}:pov:0:LEFT`).
- UTF-8 BOM + CRLF + out-of-range clamping (`brake_threshold`, `stop_speed_kmh`, `cooldown_ms`) matching C++ Task 2 tests (Review Focus #4).
- `Ets2PathDetector.InstallPluginFiles` copies DLL + WAV into `win_x64/plugins/` and preserves existing `ets2_auto_shift_lock.ini` intact.

- [ ] **Step 2: Run `dotnet test` to verify tests fail**

Run: `dotnet test configurator/AutoShiftLockConfig.sln`
Expected: FAIL (model/service classes not yet implemented).

- [ ] **Step 3: Implement `BindingSpec.cs`, `PluginConfig.cs`, and `Ets2PathDetector.cs`**

Implement exact 1:1 INI parsing/serialization and Steam library folder (`libraryfolders.vdf` + common drive paths) ETS2 detection + safe installer.

- [ ] **Step 4: Run `dotnet test` to verify tests pass**

Run: `dotnet test configurator/AutoShiftLockConfig.sln`
Expected: PASS.

- [ ] **Step 5: Implement `UniversalInputCapture.cs`, `MainForm.cs`, and `Program.cs`**

Build clean Turkish WinForms UI (`ETS2 Otomatik Vites Fren Kilidi — Ayarlar`):
- **Kurulum bölümü:** Algılanan ETS2 `win_x64\plugins` yolu + Gözat + **"Eklentiyi Kur / Güncelle"** butonu + kurulu durum etiketi.
- **Tuş / Buton Atamaları bölümü:** 5 satır (`Vites Yükselt (R→N→D)`, `Vites Düşür (D→N→R)`, `Doğrudan D (İleri)`, `Doğrudan N (Boş)`, `Doğrudan R (Geri)`); her satırda okunabilir atama etiketi, **"Tuşa Basarak Ata"** (3–5 sn dinleme / ESC ile iptal) ve **"Temizle"** düğmesi.
- **Kilit Eşikleri bölümü:** Fren eşiği (`%5–100`, varsayılan `%25`), Durma hızı toleransı (`0.0–10.0 km/h`, varsayılan `0.5`), Fren kaynağı (`Ayak Freni Pedalı` / `Efektif Fren`).
- **Sesli Uyarı bölümü:** Aktif kutucuğu, `.wav` dosya yolu + Gözat, Bekleme süresi (`ms`), **"Sesi Test Et"** düğmesi.
- **Alt bar:** Kısa kurulum hatırlatması (*"ETS2 Ayarlar → Tuşlar ve Düğmeler'den mevcut Vites Yükselt/Düşür tuşlarını kaldırın, vites tipini 'Gerçek Otomatik' seçin."*) + **"Ayarları Kaydet"** düğmesi.

- [ ] **Step 6: Write `scripts/build_release.ps1` and `README.md`, then run end-to-end build & package verification**

`scripts/build_release.ps1`:
1. Configures & builds x64 `ets2_auto_shift_lock.dll` + runs `ctest`.
2. Runs `python assets/generate_lock_wav.py` if WAV missing.
3. Runs `dotnet test configurator/AutoShiftLockConfig.sln`.
4. Publishes `AutoShiftLockConfig.exe` (`dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true`).
5. Stages `AutoShiftLockConfig.exe` + `plugin/ets2_auto_shift_lock.dll` + `plugin/lock_warning.wav` + `README.md` into `dist/ets2-auto-shift-lock-v1.0.0-win-x64.zip`.

Run: `powershell -ExecutionPolicy Bypass -File scripts/build_release.ps1`
Expected: C++ tests pass, .NET tests pass, x64 DLL + self-contained EXE built, ZIP created in `dist/` containing all expected files.

- [ ] **Step 7: Commit**

```bash
git add configurator/ scripts/build_release.ps1 README.md
git commit -m "feat(configurator): add .NET 9 setup/binding UI, tests, release bundler, and README"
```
