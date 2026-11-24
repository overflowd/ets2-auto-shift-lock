# Official SCS Software Telemetry & Input SDK 1.14 Headers

- **Source:** `https://download.eurotrucksimulator2.com/scs_sdk_1_14.zip` (SCS Software official SDK distribution)
- **License:** See `sdk_license.txt` in this directory.
- **Verified API details:**
  - Telemetry API version: `SCS_TELEMETRY_VERSION_1_01` (`1.01`)
  - Input API version: `SCS_INPUT_VERSION_1_00` (`1.00`)
  - Telemetry config: `SCS_TELEMETRY_CONFIG_controls` (`"controls"`) -> `SCS_TELEMETRY_CONFIG_ATTRIBUTE_shifter_type` (`"shifter.type"`): `"arcade"`, `"automatic"`, `"manual"`, `"hshifter"`
  - Telemetry channels:
    - `SCS_TELEMETRY_TRUCK_CHANNEL_speed` (`"truck.speed"`, `float`, m/s)
    - `SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear` (`"truck.engine.gear"`, `s32`)
    - `SCS_TELEMETRY_TRUCK_CHANNEL_displayed_gear` (`"truck.displayed.gear"`, `s32`)
    - `SCS_TELEMETRY_TRUCK_CHANNEL_input_brake` (`"truck.input.brake"`, `float`, 0..1)
    - `SCS_TELEMETRY_TRUCK_CHANNEL_effective_brake` (`"truck.effective.brake"`, `float`, 0..1)
  - Semantical input device (`SCS_INPUT_DEVICE_TYPE_semantical`):
    - Boolean inputs `"gearup"` and `"geardown"` map directly to ETS2's built-in `semantical.gearup?0` and `semantical.geardown?0` mixes in `controls.sii`.
