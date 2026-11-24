#include "test_framework.hpp"

#include "binding_types.hpp"
#include "config_store.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace ets2asl;

TEST_CASE(config_defaults_match_spec) {
    const PluginConfig cfg = parse_ini_text("");
    CHECK_NEAR(cfg.brake_threshold, 0.25f, 1e-4f);
    CHECK_NEAR(cfg.stop_speed_kmh, 0.5f, 1e-4f);
    CHECK(cfg.brake_source == BrakeSource::Input);
    CHECK(cfg.audio_enabled == true);
    CHECK_EQ(cfg.audio_sound_file, std::string("lock_warning.wav"));
    CHECK_EQ(cfg.audio_cooldown_ms, 350u);
    CHECK_EQ(cfg.pulse_ms, 60u);
    CHECK_EQ(cfg.pulse_gap_ms, 80u);
    for (const auto& b : cfg.bindings) {
        CHECK(b.source == BindingSource::None);
    }
}

TEST_CASE(binding_token_round_trip_all_sources) {
    const std::string tokens[] = {
        "kb:160",
        "xinput:0:RB",
        "xinput:2:LT",
        "dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:btn:4",
        "dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:pov:0:UP",
        "dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:pov:1:LEFT",
    };

    for (const auto& tok : tokens) {
        const BindingSpec parsed = parse_binding_token(tok);
        CHECK(parsed.source != BindingSource::None);
        CHECK_EQ(format_binding_token(parsed), tok);
    }

    // Lowercase GUID normalizes to uppercase
    const BindingSpec lower_guid = parse_binding_token("dinput:12345678-9abc-def0-1234-56789abcdef0:btn:7");
    CHECK(lower_guid.source == BindingSource::DirectInputButton);
    CHECK_EQ(lower_guid.device_guid, std::string("12345678-9ABC-DEF0-1234-56789ABCDEF0"));
    CHECK_EQ(lower_guid.index, 7u);

    // Invalid / empty tokens
    CHECK(parse_binding_token("").source == BindingSource::None);
    CHECK(parse_binding_token("   ").source == BindingSource::None);
    CHECK(parse_binding_token("kb:0").source == BindingSource::None);
    CHECK(parse_binding_token("kb:999").source == BindingSource::None);
    CHECK(parse_binding_token("xinput:4:A").source == BindingSource::None);
    CHECK(parse_binding_token("xinput:0:NOPE").source == BindingSource::None);
    CHECK(parse_binding_token("dinput:bad:xyz").source == BindingSource::None);
    CHECK(parse_binding_token("dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:pov:0:DIAG").source == BindingSource::None);
}

TEST_CASE(config_handles_bom_crlf_comments_and_clamps_invalid_values) {
    const std::string raw =
        "\xEF\xBB\xBF; header comment\r\n"
        "[THRESHOLDS]\r\n"
        "brake_threshold = 2.5\r\n"
        "stop_speed_kmh = -3.0\r\n"
        "brake_source = effective\r\n"
        "# another comment\r\n"
        "[audio]\r\n"
        "enabled = false\r\n"
        "sound_file = custom_click.wav\r\n"
        "cooldown_ms = 99999\r\n"
        "[timing]\r\n"
        "pulse_ms = 5\r\n"
        "pulse_gap_ms = 900\r\n"
        "[bindings]\r\n"
        "shift_up = kb:160\r\n"
        "shift_down = xinput:0:LB\r\n"
        "gear_d = garbage_token\r\n"
        "gear_n = dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:btn:2\r\n"
        "gear_r = dinput:12345678-9ABC-DEF0-1234-56789ABCDEF0:pov:0:DOWN\r\n";

    const PluginConfig cfg = parse_ini_text(raw);
    CHECK_NEAR(cfg.brake_threshold, 1.0f, 1e-4f);
    CHECK_NEAR(cfg.stop_speed_kmh, 0.0f, 1e-4f);
    CHECK(cfg.brake_source == BrakeSource::Effective);
    CHECK(cfg.audio_enabled == false);
    CHECK_EQ(cfg.audio_sound_file, std::string("custom_click.wav"));
    CHECK_EQ(cfg.audio_cooldown_ms, 5000u);
    CHECK_EQ(cfg.pulse_ms, 20u);
    CHECK_EQ(cfg.pulse_gap_ms, 500u);

    CHECK(cfg.bindings[static_cast<size_t>(ShiftAction::ShiftUp)].source == BindingSource::Keyboard);
    CHECK_EQ(cfg.bindings[static_cast<size_t>(ShiftAction::ShiftUp)].vk_code, 160u);
    CHECK(cfg.bindings[static_cast<size_t>(ShiftAction::ShiftDown)].source == BindingSource::XInput);
    CHECK(cfg.bindings[static_cast<size_t>(ShiftAction::GearD)].source == BindingSource::None);
    CHECK(cfg.bindings[static_cast<size_t>(ShiftAction::GearN)].source == BindingSource::DirectInputButton);
    CHECK(cfg.bindings[static_cast<size_t>(ShiftAction::GearR)].source == BindingSource::DirectInputPov);

    // Lower clamp check
    const PluginConfig low = parse_ini_text("[thresholds]\nbrake_threshold = -0.5\nstop_speed_kmh = 99.0\n");
    CHECK_NEAR(low.brake_threshold, 0.01f, 1e-4f);
    CHECK_NEAR(low.stop_speed_kmh, 10.0f, 1e-4f);
}

TEST_CASE(config_serialize_round_trip) {
    PluginConfig orig;
    orig.brake_threshold = 0.35f;
    orig.stop_speed_kmh = 0.75f;
    orig.brake_source = BrakeSource::Effective;
    orig.audio_enabled = false;
    orig.audio_sound_file = "my_alert.wav";
    orig.audio_cooldown_ms = 420;
    orig.pulse_ms = 75;
    orig.pulse_gap_ms = 95;
    orig.bindings[static_cast<size_t>(ShiftAction::ShiftUp)] = parse_binding_token("kb:160");
    orig.bindings[static_cast<size_t>(ShiftAction::GearR)] = parse_binding_token("xinput:1:RT");

    const std::string text = serialize_ini_text(orig);
    const PluginConfig reparsed = parse_ini_text(text);

    CHECK_NEAR(reparsed.brake_threshold, 0.35f, 1e-3f);
    CHECK_NEAR(reparsed.stop_speed_kmh, 0.75f, 1e-3f);
    CHECK(reparsed.brake_source == BrakeSource::Effective);
    CHECK(reparsed.audio_enabled == false);
    CHECK_EQ(reparsed.audio_sound_file, std::string("my_alert.wav"));
    CHECK_EQ(reparsed.audio_cooldown_ms, 420u);
    CHECK_EQ(reparsed.pulse_ms, 75u);
    CHECK_EQ(reparsed.pulse_gap_ms, 95u);
    CHECK_EQ(format_binding_token(reparsed.bindings[static_cast<size_t>(ShiftAction::ShiftUp)]), std::string("kb:160"));
    CHECK_EQ(format_binding_token(reparsed.bindings[static_cast<size_t>(ShiftAction::GearR)]), std::string("xinput:1:RT"));
}

TEST_CASE(config_watcher_reloads_on_mtime_change_and_handles_missing_file) {
    const auto tmp_path = std::filesystem::temp_directory_path() / "ets2asl_test_watcher.ini";
    std::error_code ec;
    std::filesystem::remove(tmp_path, ec);

    ConfigWatcher watcher(tmp_path);
    PluginConfig cfg;
    // Missing file returns false cleanly without throwing
    CHECK(watcher.reload_if_changed(cfg) == false);

    {
        std::ofstream f(tmp_path, std::ios::binary);
        f << "[bindings]\nshift_up = kb:160\n";
    }
    CHECK(watcher.reload_if_changed(cfg) == true);
    CHECK_EQ(cfg.bindings[static_cast<size_t>(ShiftAction::ShiftUp)].vk_code, 160u);

    // Unchanged file does not reload
    CHECK(watcher.reload_if_changed(cfg) == false);

    // Update content and bump mtime explicitly
    {
        std::ofstream f(tmp_path, std::ios::binary);
        f << "[bindings]\nshift_up = kb:162\n";
    }
    const auto old_time = std::filesystem::last_write_time(tmp_path, ec);
    std::filesystem::last_write_time(tmp_path, old_time + std::chrono::seconds(2), ec);

    CHECK(watcher.reload_if_changed(cfg) == true);
    CHECK_EQ(cfg.bindings[static_cast<size_t>(ShiftAction::ShiftUp)].vk_code, 162u);

    std::filesystem::remove(tmp_path, ec);
}
