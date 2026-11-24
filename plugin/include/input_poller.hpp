#pragma once

#include "binding_types.hpp"
#include "config_store.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct IDirectInput8W;
struct IDirectInputDevice8W;

namespace ets2asl {

class InputPoller {
public:
    InputPoller();
    ~InputPoller();

    InputPoller(const InputPoller&) = delete;
    InputPoller& operator=(const InputPoller&) = delete;

    void initialize(HINSTANCE module_instance);
    void shutdown();
    void reset_edges();

    std::array<bool, kShiftActionCount> poll_pressed_edges(const PluginConfig& cfg,
                                                           bool allow_capture,
                                                           uint64_t now_ms);

    static bool eval_pov_direction(uint32_t raw_pov_hundredths, PovDirection dir);
    static bool eval_xinput_button(const XINPUT_GAMEPAD& gp, std::string_view button_name);
    static std::string format_guid(const GUID& guid);

private:
    struct DiDevice {
        IDirectInputDevice8W* device = nullptr;
        std::string product_guid;
        bool state_valid = false;
        std::array<bool, 128> buttons{};
        std::array<uint32_t, 4> povs{0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    };

    static bool is_current_process_foreground();
    static BOOL CALLBACK enum_devices_cb(const void* instance_ptr, void* context);

    void enumerate_dinput_devices();
    void release_dinput_devices();
    void poll_dinput_states(uint64_t now_ms, const PluginConfig& cfg);
    bool is_binding_down(const BindingSpec& spec,
                         const std::array<bool, 4>& xinput_valid,
                         const std::array<XINPUT_STATE, 4>& xinput_states) const;

    using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

    HINSTANCE module_instance_ = nullptr;
    HMODULE xinput_dll_ = nullptr;
    XInputGetStateFn xinput_get_state_ = nullptr;

    IDirectInput8W* dinput_ = nullptr;
    std::vector<DiDevice> di_devices_;
    uint64_t last_reenum_ms_ = 0;

    std::array<bool, kShiftActionCount> prev_down_{};
    bool priming_ = true;
};

}  // namespace ets2asl
