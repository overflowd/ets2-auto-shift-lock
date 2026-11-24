#include "test_framework.hpp"

#include "shift_lock_engine.hpp"

using namespace ets2asl;

namespace {

TelemetrySnapshot make_auto_stopped(int32_t gear, int32_t displayed_gear, float input_brake) {
    TelemetrySnapshot t;
    t.mode = ShifterMode::Automatic;
    t.paused = false;
    t.input_brake = input_brake;
    t.effective_brake = input_brake;
    t.speed_mps = 0.0f;
    t.gear = gear;
    t.displayed_gear = displayed_gear;
    return t;
}

}  // namespace

TEST_CASE(shifter_mode_parsing_matches_scs_strings) {
    CHECK(parse_shifter_mode("arcade") == ShifterMode::Arcade);
    CHECK(parse_shifter_mode("automatic") == ShifterMode::Automatic);
    CHECK(parse_shifter_mode("manual") == ShifterMode::Manual);
    CHECK(parse_shifter_mode("hshifter") == ShifterMode::HShifter);
    CHECK(parse_shifter_mode("") == ShifterMode::Unknown);
    CHECK(parse_shifter_mode("weird") == ShifterMode::Unknown);
}

TEST_CASE(automatic_blocks_all_dnr_transitions_without_brake_and_fires_audio) {
    const PluginConfig cfg;
    struct Case {
        int32_t gear;
        ShiftAction action;
    };
    const Case cases[] = {
        {0, ShiftAction::ShiftUp},    // N -> D
        {0, ShiftAction::GearD},      // N -> D
        {0, ShiftAction::ShiftDown},  // N -> R
        {0, ShiftAction::GearR},      // N -> R
        {1, ShiftAction::ShiftDown},  // D -> N
        {1, ShiftAction::GearN},      // D -> N
        {1, ShiftAction::GearR},      // D -> R
        {-1, ShiftAction::ShiftUp},   // R -> N
        {-1, ShiftAction::GearN},     // R -> N
        {-1, ShiftAction::GearD},     // R -> D
    };

    for (const auto& c : cases) {
        ShiftLockEngine engine;
        const auto t = make_auto_stopped(c.gear, c.gear, 0.10f);  // < 0.25
        engine.on_action_pressed(c.action, t, cfg, 1000);
        const EngineOutput out = engine.tick(t, cfg, 1000);
        CHECK(out.gearup_active == false);
        CHECK(out.geardown_active == false);
        CHECK(out.trigger_lock_audio == true);
        // One-shot audio flag clears on next tick
        const EngineOutput out2 = engine.tick(t, cfg, 1016);
        CHECK(out2.trigger_lock_audio == false);
    }
}

TEST_CASE(automatic_blocks_shift_when_moving_above_tolerance_even_with_brake) {
    const PluginConfig cfg;  // stop_speed_kmh = 0.5, brake_threshold = 0.25
    ShiftLockEngine engine;

    // 0.30 m/s = 1.08 km/h > 0.5 km/h -> blocked
    auto t = make_auto_stopped(0, 0, 0.80f);
    t.speed_mps = 0.30f;
    engine.on_action_pressed(ShiftAction::ShiftUp, t, cfg, 1000);
    auto out = engine.tick(t, cfg, 1000);
    CHECK(out.gearup_active == false);
    CHECK(out.trigger_lock_audio == true);

    // Reverse rolling -0.25 m/s = 0.90 km/h > 0.5 km/h -> blocked
    t.speed_mps = -0.25f;
    engine.on_action_pressed(ShiftAction::ShiftUp, t, cfg, 1100);
    out = engine.tick(t, cfg, 1100);
    CHECK(out.gearup_active == false);
    CHECK(out.trigger_lock_audio == true);

    // Physics jitter 0.10 m/s = 0.36 km/h <= 0.5 km/h with brake 0.30 -> allowed
    t.speed_mps = 0.10f;
    t.input_brake = 0.30f;
    engine.on_action_pressed(ShiftAction::ShiftUp, t, cfg, 1200);
    out = engine.tick(t, cfg, 1200);
    CHECK(out.gearup_active == true);
    CHECK(out.geardown_active == false);
    CHECK(out.trigger_lock_audio == false);
}

TEST_CASE(same_range_press_is_silent_noop) {
    const PluginConfig cfg;
    ShiftLockEngine engine;

    // In D, moving fast with no brake: GearD and ShiftUp are silent no-ops
    auto t = make_auto_stopped(5, 5, 0.0f);
    t.speed_mps = 20.0f;
    engine.on_action_pressed(ShiftAction::GearD, t, cfg, 1000);
    auto out = engine.tick(t, cfg, 1000);
    CHECK(!out.gearup_active && !out.geardown_active && !out.trigger_lock_audio);

    engine.on_action_pressed(ShiftAction::ShiftUp, t, cfg, 1050);
    out = engine.tick(t, cfg, 1050);
    CHECK(!out.gearup_active && !out.geardown_active && !out.trigger_lock_audio);

    // In R: GearR and ShiftDown are silent no-ops
    t = make_auto_stopped(-1, -1, 0.0f);
    engine.on_action_pressed(ShiftAction::GearR, t, cfg, 1100);
    out = engine.tick(t, cfg, 1100);
    CHECK(!out.gearup_active && !out.geardown_active && !out.trigger_lock_audio);

    engine.on_action_pressed(ShiftAction::ShiftDown, t, cfg, 1150);
    out = engine.tick(t, cfg, 1150);
    CHECK(!out.gearup_active && !out.geardown_active && !out.trigger_lock_audio);

    // In N: GearN is silent no-op
    t = make_auto_stopped(0, 0, 0.0f);
    engine.on_action_pressed(ShiftAction::GearN, t, cfg, 1200);
    out = engine.tick(t, cfg, 1200);
    CHECK(!out.gearup_active && !out.geardown_active && !out.trigger_lock_audio);
}

TEST_CASE(direct_d_to_r_emits_two_spaced_geardown_pulses) {
    PluginConfig cfg;  // pulse_ms=60, pulse_gap_ms=80
    ShiftLockEngine engine;
    const auto t = make_auto_stopped(1, 1, 0.80f);

    engine.on_action_pressed(ShiftAction::GearR, t, cfg, 1000);
    CHECK(engine.tick(t, cfg, 1000).geardown_active == true);
    CHECK(engine.tick(t, cfg, 1059).geardown_active == true);
    // Gap 1060..1139
    CHECK(engine.tick(t, cfg, 1060).geardown_active == false);
    CHECK(engine.tick(t, cfg, 1139).geardown_active == false);
    // Second pulse 1140..1199
    CHECK(engine.tick(t, cfg, 1140).geardown_active == true);
    CHECK(engine.tick(t, cfg, 1199).geardown_active == true);
    // Done at 1200
    const auto done = engine.tick(t, cfg, 1200);
    CHECK(done.gearup_active == false);
    CHECK(done.geardown_active == false);
}

TEST_CASE(mid_transition_d_to_r_aborts_second_pulse_if_brake_released_or_moving) {
    // Review Focus #1
    const PluginConfig cfg;  // pulse_ms=60, pulse_gap_ms=80
    ShiftLockEngine engine;
    auto t = make_auto_stopped(1, 1, 0.80f);

    engine.on_action_pressed(ShiftAction::GearR, t, cfg, 1000);
    CHECK(engine.tick(t, cfg, 1000).geardown_active == true);
    // Release brake during gap
    t.input_brake = 0.0f;
    CHECK(engine.tick(t, cfg, 1070).geardown_active == false);
    // Second pulse time reached -> MUST be aborted
    const auto out = engine.tick(t, cfg, 1140);
    CHECK(out.geardown_active == false);
    CHECK(out.gearup_active == false);
}

TEST_CASE(stationary_d_displayed_gear_beats_zero_raw_gear) {
    // Review Focus #2
    CHECK(derive_gear_range(0, 1) == GearRange::Drive);
    CHECK(derive_gear_range(0, -1) == GearRange::Reverse);
    CHECK(derive_gear_range(2, 0) == GearRange::Drive);
    CHECK(derive_gear_range(0, 0) == GearRange::Neutral);

    const PluginConfig cfg;
    ShiftLockEngine engine;
    const auto t = make_auto_stopped(0, 1, 0.0f);  // stopped in D, raw gear=0, displayed=1
    engine.on_action_pressed(ShiftAction::GearD, t, cfg, 1000);
    auto out = engine.tick(t, cfg, 1000);
    CHECK(!out.gearup_active && !out.geardown_active && !out.trigger_lock_audio);

    // Press N with brake -> emits single geardown pulse
    const auto t_brake = make_auto_stopped(0, 1, 0.50f);
    engine.on_action_pressed(ShiftAction::GearN, t_brake, cfg, 1100);
    out = engine.tick(t_brake, cfg, 1100);
    CHECK(out.geardown_active == true);
    CHECK(out.trigger_lock_audio == false);
}

TEST_CASE(manual_mode_passes_shift_up_down_without_lock_and_ignores_dnr) {
    const PluginConfig cfg;
    ShiftLockEngine engine;
    TelemetrySnapshot t;
    t.mode = ShifterMode::Manual;
    t.paused = false;
    t.input_brake = 0.0f;
    t.speed_mps = 25.0f;
    t.gear = 4;
    t.displayed_gear = 4;

    engine.on_action_pressed(ShiftAction::ShiftUp, t, cfg, 1000);
    auto out = engine.tick(t, cfg, 1000);
    CHECK(out.gearup_active == true);
    CHECK(out.trigger_lock_audio == false);

    // Finish pulse
    CHECK(engine.tick(t, cfg, 1060).gearup_active == false);

    engine.on_action_pressed(ShiftAction::ShiftDown, t, cfg, 1100);
    out = engine.tick(t, cfg, 1100);
    CHECK(out.geardown_active == true);
    CHECK(out.trigger_lock_audio == false);
    CHECK(engine.tick(t, cfg, 1160).geardown_active == false);

    // Direct D/N/R ignored in manual mode
    for (ShiftAction act : {ShiftAction::GearD, ShiftAction::GearN, ShiftAction::GearR}) {
        engine.on_action_pressed(act, t, cfg, 1200);
        out = engine.tick(t, cfg, 1200);
        CHECK(!out.gearup_active && !out.geardown_active && !out.trigger_lock_audio);
    }
}

TEST_CASE(arcade_and_hshifter_modes_are_completely_inactive) {
    const PluginConfig cfg;
    for (ShifterMode mode : {ShifterMode::Arcade, ShifterMode::HShifter, ShifterMode::Unknown}) {
        ShiftLockEngine engine;
        TelemetrySnapshot t;
        t.mode = mode;
        t.paused = false;
        t.input_brake = 1.0f;
        t.speed_mps = 0.0f;

        for (ShiftAction act : {ShiftAction::ShiftUp, ShiftAction::ShiftDown,
                                ShiftAction::GearD, ShiftAction::GearN, ShiftAction::GearR}) {
            engine.on_action_pressed(act, t, cfg, 1000);
            const auto out = engine.tick(t, cfg, 1000);
            CHECK(!out.gearup_active && !out.geardown_active && !out.trigger_lock_audio);
        }
    }
}

TEST_CASE(pause_immediately_clears_active_pulse_and_pending_queue) {
    // Review Focus #3
    const PluginConfig cfg;
    ShiftLockEngine engine;
    auto t = make_auto_stopped(1, 1, 0.80f);

    engine.on_action_pressed(ShiftAction::GearR, t, cfg, 1000);
    CHECK(engine.tick(t, cfg, 1000).geardown_active == true);

    t.paused = true;
    auto out = engine.tick(t, cfg, 1010);
    CHECK(out.geardown_active == false);
    CHECK(out.gearup_active == false);

    // Unpause -> stays cleared (no ghost pulse)
    t.paused = false;
    out = engine.tick(t, cfg, 1020);
    CHECK(out.geardown_active == false);
    CHECK(out.gearup_active == false);
}

TEST_CASE(brake_source_effective_option_is_respected) {
    PluginConfig cfg;
    cfg.brake_source = BrakeSource::Effective;
    ShiftLockEngine engine;

    auto t = make_auto_stopped(0, 0, 0.0f);
    t.input_brake = 0.0f;
    t.effective_brake = 0.60f;
    engine.on_action_pressed(ShiftAction::ShiftUp, t, cfg, 1000);
    const auto out = engine.tick(t, cfg, 1000);
    CHECK(out.gearup_active == true);
    CHECK(out.trigger_lock_audio == false);
}

TEST_CASE(focus_loss_without_pause_immediately_aborts_active_pulse_and_queue) {
    // Review Focus #3 (Convoy / g_pause_on_alt_tab 0)
    const PluginConfig cfg;
    ShiftLockEngine engine;
    auto t = make_auto_stopped(1, 1, 0.80f);
    t.focused = true;

    engine.on_action_pressed(ShiftAction::GearR, t, cfg, 1000);
    CHECK(engine.tick(t, cfg, 1000).geardown_active == true);

    // Lose foreground focus without game pausing
    t.focused = false;
    auto out = engine.tick(t, cfg, 1015);
    CHECK(out.geardown_active == false);
    CHECK(out.gearup_active == false);

    // Regain focus later -> pending pulse 2 must NOT fire
    t.focused = true;
    out = engine.tick(t, cfg, 1150);
    CHECK(out.geardown_active == false);
    CHECK(out.gearup_active == false);
}
