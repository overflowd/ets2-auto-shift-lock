#pragma once

#include <cstdint>
#include <string>

namespace ets2asl {

enum class ShiftAction : uint8_t {
    ShiftUp = 0,
    ShiftDown = 1,
    GearD = 2,
    GearN = 3,
    GearR = 4,
    Count = 5,
};

constexpr size_t kShiftActionCount = static_cast<size_t>(ShiftAction::Count);

enum class BindingSource : uint8_t {
    None = 0,
    Keyboard,
    XInput,
    DirectInputButton,
    DirectInputPov,
};

enum class PovDirection : uint8_t {
    Up = 0,
    Right,
    Down,
    Left,
};

enum class BrakeSource : uint8_t {
    Input = 0,
    Effective,
};

struct BindingSpec {
    BindingSource source = BindingSource::None;
    uint32_t vk_code = 0;
    uint8_t xinput_slot = 0;
    std::string xinput_button;
    std::string device_guid;
    uint32_t index = 0;
    PovDirection pov_dir = PovDirection::Up;
};

}  // namespace ets2asl
