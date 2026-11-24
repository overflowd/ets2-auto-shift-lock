#include "test_framework.hpp"

#include "input_poller.hpp"

using namespace ets2asl;

TEST_CASE(pov_direction_matching_handles_center_and_diagonals) {
    CHECK(InputPoller::eval_pov_direction(0xFFFFFFFFu, PovDirection::Up) == false);
    CHECK(InputPoller::eval_pov_direction(0xFFFFu, PovDirection::Down) == false);

    CHECK(InputPoller::eval_pov_direction(0u, PovDirection::Up) == true);
    CHECK(InputPoller::eval_pov_direction(0u, PovDirection::Right) == false);

    CHECK(InputPoller::eval_pov_direction(9000u, PovDirection::Right) == true);
    CHECK(InputPoller::eval_pov_direction(9000u, PovDirection::Up) == false);

    CHECK(InputPoller::eval_pov_direction(18000u, PovDirection::Down) == true);
    CHECK(InputPoller::eval_pov_direction(27000u, PovDirection::Left) == true);

    // Diagonals activate adjacent cardinal directions
    CHECK(InputPoller::eval_pov_direction(4500u, PovDirection::Up) == true);
    CHECK(InputPoller::eval_pov_direction(4500u, PovDirection::Right) == true);
    CHECK(InputPoller::eval_pov_direction(4500u, PovDirection::Down) == false);

    CHECK(InputPoller::eval_pov_direction(31500u, PovDirection::Up) == true);
    CHECK(InputPoller::eval_pov_direction(31500u, PovDirection::Left) == true);
}

TEST_CASE(xinput_button_eval_supports_digital_and_analog_triggers) {
    XINPUT_GAMEPAD gp{};
    gp.wButtons = XINPUT_GAMEPAD_RIGHT_SHOULDER | XINPUT_GAMEPAD_A;
    gp.bLeftTrigger = 120;
    gp.bRightTrigger = 200;

    CHECK(InputPoller::eval_xinput_button(gp, "RB") == true);
    CHECK(InputPoller::eval_xinput_button(gp, "A") == true);
    CHECK(InputPoller::eval_xinput_button(gp, "LB") == false);
    CHECK(InputPoller::eval_xinput_button(gp, "LT") == false);
    CHECK(InputPoller::eval_xinput_button(gp, "RT") == true);

    gp.bLeftTrigger = 128;
    CHECK(InputPoller::eval_xinput_button(gp, "LT") == true);
}

TEST_CASE(allow_capture_false_resets_edges_without_firing) {
    // Review Focus #3
    InputPoller poller;
    PluginConfig cfg;
    cfg.bindings[static_cast<size_t>(ShiftAction::ShiftUp)] = parse_binding_token("kb:160");

    const auto edges = poller.poll_pressed_edges(cfg, /*allow_capture=*/false, 1000);
    for (bool pressed : edges) {
        CHECK(pressed == false);
    }
}

TEST_CASE(guid_formatting_produces_uppercase_hyphenated_canonical_string) {
    GUID g{};
    g.Data1 = 0x12345678;
    g.Data2 = 0x9ABC;
    g.Data3 = 0xDEF0;
    g.Data4[0] = 0x12;
    g.Data4[1] = 0x34;
    g.Data4[2] = 0x56;
    g.Data4[3] = 0x78;
    g.Data4[4] = 0x9A;
    g.Data4[5] = 0xBC;
    g.Data4[6] = 0xDE;
    g.Data4[7] = 0xF0;

    CHECK_EQ(InputPoller::format_guid(g), std::string("12345678-9ABC-DEF0-1234-56789ABCDEF0"));
}

TEST_CASE(dinput_reenum_triggers_on_unhealthy_device_after_interval) {
    // Healthy and present -> no re-enum
    CHECK(InputPoller::should_reenumerate_dinput(false, false, 5000, 1000) == false);
    // Unhealthy device (e.g. lost/unplugged/unacquired) triggers after 3000ms interval
    CHECK(InputPoller::should_reenumerate_dinput(false, true, 2500, 1000) == false);
    CHECK(InputPoller::should_reenumerate_dinput(false, true, 4000, 1000) == true);
    // First-time missing or unhealthy (last_reenum_ms == 0) triggers immediately
    CHECK(InputPoller::should_reenumerate_dinput(false, true, 100, 0) == true);
    CHECK(InputPoller::should_reenumerate_dinput(true, false, 100, 0) == true);
}
