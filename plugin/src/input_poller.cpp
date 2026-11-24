#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include "input_poller.hpp"

#include <dinput.h>

#include <cstdio>
#include <cstring>

namespace ets2asl {
namespace {

constexpr uint8_t kTriggerThreshold = 128;  // 50% of 0..255
constexpr uint64_t kReenumIntervalMs = 3000;

struct FindWindowCtx {
    DWORD pid = 0;
    HWND hwnd = nullptr;
};

BOOL CALLBACK find_process_window_cb(HWND hwnd, LPARAM lparam) {
    auto* ctx = reinterpret_cast<FindWindowCtx*>(lparam);
    DWORD win_pid = 0;
    GetWindowThreadProcessId(hwnd, &win_pid);
    if (win_pid == ctx->pid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
        ctx->hwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND find_main_process_window() {
    FindWindowCtx ctx{GetCurrentProcessId(), nullptr};
    EnumWindows(find_process_window_cb, reinterpret_cast<LPARAM>(&ctx));
    return ctx.hwnd;
}

}  // namespace

InputPoller::InputPoller() = default;

InputPoller::~InputPoller() {
    shutdown();
}

void InputPoller::initialize(HINSTANCE module_instance) {
    shutdown();
    module_instance_ = module_instance ? module_instance : GetModuleHandleW(nullptr);

    // Load XInput dynamically so we work across Windows 8/10/11 regardless of runtime DLL
    const wchar_t* const kXInputDlls[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
    for (const wchar_t* name : kXInputDlls) {
        xinput_dll_ = LoadLibraryW(name);
        if (xinput_dll_) {
            xinput_get_state_ = reinterpret_cast<XInputGetStateFn>(
                GetProcAddress(xinput_dll_, "XInputGetState"));
            if (xinput_get_state_) {
                break;
            }
            FreeLibrary(xinput_dll_);
            xinput_dll_ = nullptr;
        }
    }

    // Initialize DirectInput8 for universal wheel / shifter / button-box support
    const HRESULT hr = DirectInput8Create(
        module_instance_,
        DIRECTINPUT_VERSION,
        IID_IDirectInput8W,
        reinterpret_cast<void**>(&dinput_),
        nullptr);
    if (SUCCEEDED(hr) && dinput_) {
        enumerate_dinput_devices();
    } else {
        dinput_ = nullptr;
    }

    reset_edges();
}

void InputPoller::shutdown() {
    release_dinput_devices();
    if (dinput_) {
        dinput_->Release();
        dinput_ = nullptr;
    }
    if (xinput_dll_) {
        FreeLibrary(xinput_dll_);
        xinput_dll_ = nullptr;
    }
    xinput_get_state_ = nullptr;
    reset_edges();
}

void InputPoller::reset_edges() {
    prev_down_.fill(false);
    priming_ = true;
}

bool InputPoller::is_current_process_foreground() {
    const HWND fg = GetForegroundWindow();
    if (!fg) {
        return false;
    }
    DWORD fg_pid = 0;
    GetWindowThreadProcessId(fg, &fg_pid);
    return fg_pid == GetCurrentProcessId();
}

std::string InputPoller::format_guid(const GUID& guid) {
    char buf[40] = {};
    std::snprintf(
        buf, sizeof(buf),
        "%08lX-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X",
        static_cast<unsigned long>(guid.Data1),
        static_cast<unsigned short>(guid.Data2),
        static_cast<unsigned short>(guid.Data3),
        static_cast<unsigned int>(guid.Data4[0]),
        static_cast<unsigned int>(guid.Data4[1]),
        static_cast<unsigned int>(guid.Data4[2]),
        static_cast<unsigned int>(guid.Data4[3]),
        static_cast<unsigned int>(guid.Data4[4]),
        static_cast<unsigned int>(guid.Data4[5]),
        static_cast<unsigned int>(guid.Data4[6]),
        static_cast<unsigned int>(guid.Data4[7]));
    return std::string(buf);
}

BOOL CALLBACK InputPoller::enum_devices_cb(const void* instance_ptr, void* context) {
    auto* self = static_cast<InputPoller*>(context);
    const auto* inst = static_cast<const DIDEVICEINSTANCEW*>(instance_ptr);
    if (!self || !self->dinput_ || !inst) {
        return DIENUM_CONTINUE;
    }

    IDirectInputDevice8W* dev = nullptr;
    if (FAILED(self->dinput_->CreateDevice(inst->guidInstance, &dev, nullptr)) || !dev) {
        return DIENUM_CONTINUE;
    }

    if (FAILED(dev->SetDataFormat(&c_dfDIJoystick2))) {
        dev->Release();
        return DIENUM_CONTINUE;
    }

    DiDevice entry;
    entry.device = dev;
    entry.product_guid = format_guid(inst->guidProduct);

    if (HWND hwnd = find_main_process_window()) {
        if (SUCCEEDED(dev->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))) {
            entry.coop_set = true;
            dev->Acquire();
        }
    }

    self->di_devices_.push_back(std::move(entry));
    return DIENUM_CONTINUE;
}

bool InputPoller::should_reenumerate_dinput(bool missing_bound_guid,
                                            bool any_device_unhealthy,
                                            uint64_t now_ms,
                                            uint64_t last_reenum_ms) {
    if (!missing_bound_guid && !any_device_unhealthy) {
        return false;
    }
    return (last_reenum_ms == 0) || (now_ms >= last_reenum_ms + kReenumIntervalMs);
}

void InputPoller::enumerate_dinput_devices() {
    release_dinput_devices();
    if (!dinput_) {
        return;
    }
    dinput_->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        reinterpret_cast<LPDIENUMDEVICESCALLBACKW>(&InputPoller::enum_devices_cb),
        this,
        DIEDFL_ATTACHEDONLY);
}

void InputPoller::release_dinput_devices() {
    for (auto& d : di_devices_) {
        if (d.device) {
            d.device->Unacquire();
            d.device->Release();
            d.device = nullptr;
        }
    }
    di_devices_.clear();
}

void InputPoller::poll_dinput_states(uint64_t now_ms, const PluginConfig& cfg) {
    if (!dinput_) {
        return;
    }

    auto is_guid_bound = [&cfg](const std::string& guid) -> bool {
        for (const auto& b : cfg.bindings) {
            if ((b.source == BindingSource::DirectInputButton || b.source == BindingSource::DirectInputPov) &&
                b.device_guid == guid) {
                return true;
            }
        }
        return false;
    };

    bool needs_dinput = false;
    bool missing_bound_guid = false;
    for (const auto& b : cfg.bindings) {
        if (b.source == BindingSource::DirectInputButton || b.source == BindingSource::DirectInputPov) {
            needs_dinput = true;
            bool found = false;
            for (const auto& d : di_devices_) {
                if (d.product_guid == b.device_guid) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                missing_bound_guid = true;
            }
        }
    }

    if (!needs_dinput) {
        return;
    }

    if (should_reenumerate_dinput(missing_bound_guid, false, now_ms, last_reenum_ms_)) {
        last_reenum_ms_ = now_ms;
        enumerate_dinput_devices();
    }

    HWND main_hwnd = nullptr;
    bool any_bound_unhealthy = false;

    for (auto& d : di_devices_) {
        d.state_valid = false;
        if (!d.device || !is_guid_bound(d.product_guid)) {
            continue;
        }

        // Complete deferred SetCooperativeLevel once ETS2 creates its visible main window
        if (!d.coop_set) {
            if (!main_hwnd) {
                main_hwnd = find_main_process_window();
            }
            if (!main_hwnd) {
                continue;
            }
            if (FAILED(d.device->SetCooperativeLevel(main_hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))) {
                any_bound_unhealthy = true;
                continue;
            }
            d.coop_set = true;
            d.device->Acquire();
        }

        HRESULT hr = d.device->Poll();
        if (FAILED(hr)) {
            const HRESULT acq_hr = d.device->Acquire();
            if (FAILED(acq_hr)) {
                any_bound_unhealthy = true;
                continue;
            }
            d.device->Poll();
        }

        DIJOYSTATE2 js{};
        hr = d.device->GetDeviceState(sizeof(js), &js);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            const HRESULT acq_hr = d.device->Acquire();
            if (SUCCEEDED(acq_hr)) {
                hr = d.device->GetDeviceState(sizeof(js), &js);
            } else {
                hr = acq_hr;
            }
        }
        if (FAILED(hr)) {
            any_bound_unhealthy = true;
            continue;
        }

        d.state_valid = true;
        for (size_t i = 0; i < 128; ++i) {
            d.buttons[i] = (js.rgbButtons[i] & 0x80) != 0;
        }
        for (size_t i = 0; i < 4; ++i) {
            d.povs[i] = static_cast<uint32_t>(js.rgdwPOV[i]);
        }
    }

    if (should_reenumerate_dinput(false, any_bound_unhealthy, now_ms, last_reenum_ms_)) {
        last_reenum_ms_ = now_ms;
        enumerate_dinput_devices();
    }
}

bool InputPoller::eval_pov_direction(uint32_t raw_pov_hundredths, PovDirection dir) {
    if (raw_pov_hundredths == 0xFFFFFFFFu || (raw_pov_hundredths & 0xFFFFu) == 0xFFFFu) {
        return false;
    }
    const uint32_t deg = (raw_pov_hundredths % 36000u);
    switch (dir) {
        case PovDirection::Up:
            return (deg <= 4500u) || (deg >= 31500u);
        case PovDirection::Right:
            return (deg >= 4500u) && (deg <= 13500u);
        case PovDirection::Down:
            return (deg >= 13500u) && (deg <= 22500u);
        case PovDirection::Left:
            return (deg >= 22500u) && (deg <= 31500u);
    }
    return false;
}

bool InputPoller::eval_xinput_button(const XINPUT_GAMEPAD& gp, std::string_view button_name) {
    if (button_name == "A") return (gp.wButtons & XINPUT_GAMEPAD_A) != 0;
    if (button_name == "B") return (gp.wButtons & XINPUT_GAMEPAD_B) != 0;
    if (button_name == "X") return (gp.wButtons & XINPUT_GAMEPAD_X) != 0;
    if (button_name == "Y") return (gp.wButtons & XINPUT_GAMEPAD_Y) != 0;
    if (button_name == "LB") return (gp.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
    if (button_name == "RB") return (gp.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
    if (button_name == "BACK") return (gp.wButtons & XINPUT_GAMEPAD_BACK) != 0;
    if (button_name == "START") return (gp.wButtons & XINPUT_GAMEPAD_START) != 0;
    if (button_name == "LS") return (gp.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
    if (button_name == "RS") return (gp.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
    if (button_name == "DPAD_UP") return (gp.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
    if (button_name == "DPAD_DOWN") return (gp.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    if (button_name == "DPAD_LEFT") return (gp.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    if (button_name == "DPAD_RIGHT") return (gp.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
    if (button_name == "LT") return gp.bLeftTrigger >= kTriggerThreshold;
    if (button_name == "RT") return gp.bRightTrigger >= kTriggerThreshold;
    return false;
}

bool InputPoller::is_binding_down(const BindingSpec& spec,
                                  const std::array<bool, 4>& xinput_valid,
                                  const std::array<XINPUT_STATE, 4>& xinput_states) const {
    switch (spec.source) {
        case BindingSource::None:
            return false;
        case BindingSource::Keyboard:
            return (GetAsyncKeyState(static_cast<int>(spec.vk_code)) & 0x8000) != 0;
        case BindingSource::XInput:
            if (spec.xinput_slot < 4 && xinput_valid[spec.xinput_slot]) {
                return eval_xinput_button(xinput_states[spec.xinput_slot].Gamepad, spec.xinput_button);
            }
            return false;
        case BindingSource::DirectInputButton:
            if (spec.index >= 128) {
                return false;
            }
            for (const auto& d : di_devices_) {
                if (d.state_valid && d.product_guid == spec.device_guid && d.buttons[spec.index]) {
                    return true;
                }
            }
            return false;
        case BindingSource::DirectInputPov:
            if (spec.index >= 4) {
                return false;
            }
            for (const auto& d : di_devices_) {
                if (d.state_valid && d.product_guid == spec.device_guid &&
                    eval_pov_direction(d.povs[spec.index], spec.pov_dir)) {
                    return true;
                }
            }
            return false;
    }
    return false;
}

std::array<bool, kShiftActionCount> InputPoller::poll_pressed_edges(const PluginConfig& cfg,
                                                                    bool allow_capture,
                                                                    uint64_t now_ms) {
    std::array<bool, kShiftActionCount> edges{};
    if (!allow_capture || !is_current_process_foreground()) {
        reset_edges();
        return edges;
    }

    // Poll needed XInput slots once per frame
    std::array<bool, 4> xinput_valid{};
    std::array<XINPUT_STATE, 4> xinput_states{};
    if (xinput_get_state_) {
        std::array<bool, 4> slot_needed{};
        for (const auto& b : cfg.bindings) {
            if (b.source == BindingSource::XInput && b.xinput_slot < 4) {
                slot_needed[b.xinput_slot] = true;
            }
        }
        for (uint8_t s = 0; s < 4; ++s) {
            if (slot_needed[s]) {
                XINPUT_STATE st{};
                if (xinput_get_state_(s, &st) == ERROR_SUCCESS) {
                    xinput_valid[s] = true;
                    xinput_states[s] = st;
                }
            }
        }
    }

    poll_dinput_states(now_ms, cfg);

    std::array<bool, kShiftActionCount> curr_down{};
    for (size_t i = 0; i < kShiftActionCount; ++i) {
        curr_down[i] = is_binding_down(cfg.bindings[i], xinput_valid, xinput_states);
    }

    if (priming_) {
        prev_down_ = curr_down;
        priming_ = false;
        return edges;
    }

    for (size_t i = 0; i < kShiftActionCount; ++i) {
        edges[i] = curr_down[i] && !prev_down_[i];
    }
    prev_down_ = curr_down;
    return edges;
}

}  // namespace ets2asl
