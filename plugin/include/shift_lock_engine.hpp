#pragma once

#include "binding_types.hpp"
#include "config_store.hpp"

#include <cstdint>
#include <deque>
#include <string_view>

namespace ets2asl {

enum class ShifterMode : uint8_t {
    Unknown = 0,
    Arcade,
    Automatic,
    Manual,
    HShifter,
};

enum class GearRange : int8_t {
    Reverse = -1,
    Neutral = 0,
    Drive = 1,
};

struct TelemetrySnapshot {
    ShifterMode mode = ShifterMode::Unknown;
    bool paused = true;
    float input_brake = 0.0f;
    float effective_brake = 0.0f;
    float speed_mps = 0.0f;
    int32_t gear = 0;
    int32_t displayed_gear = 0;
};

struct EngineOutput {
    bool gearup_active = false;
    bool geardown_active = false;
    bool trigger_lock_audio = false;
};

ShifterMode parse_shifter_mode(std::string_view scs_shifter_type);
GearRange derive_gear_range(int32_t gear, int32_t displayed_gear);

class ShiftLockEngine {
public:
    void on_action_pressed(ShiftAction action,
                           const TelemetrySnapshot& t,
                           const PluginConfig& cfg,
                           uint64_t now_ms);

    EngineOutput tick(const TelemetrySnapshot& t,
                      const PluginConfig& cfg,
                      uint64_t now_ms);

    void reset();

private:
    struct PendingPulse {
        bool is_up = true;
        bool requires_lock = true;
    };

    enum class PulsePhase : uint8_t {
        Idle = 0,
        Pulsing,
        Gap,
    };

    static bool is_shift_unlocked(const TelemetrySnapshot& t, const PluginConfig& cfg);
    void enqueue_pulses(bool is_up, int count, bool requires_lock, uint64_t now_ms);

    std::deque<PendingPulse> queue_;
    PulsePhase phase_ = PulsePhase::Idle;
    PendingPulse active_pulse_{};
    uint64_t phase_start_ms_ = 0;
    bool pending_lock_audio_ = false;
};

}  // namespace ets2asl
