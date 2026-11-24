#include "shift_lock_engine.hpp"

#include <cmath>

namespace ets2asl {
namespace {

int range_to_int(GearRange r) {
    return static_cast<int>(static_cast<int8_t>(r));
}

GearRange sign_to_range(int32_t v) {
    if (v > 0) {
        return GearRange::Drive;
    }
    if (v < 0) {
        return GearRange::Reverse;
    }
    return GearRange::Neutral;
}

}  // namespace

ShifterMode parse_shifter_mode(std::string_view scs_shifter_type) {
    if (scs_shifter_type == "arcade") {
        return ShifterMode::Arcade;
    }
    if (scs_shifter_type == "automatic") {
        return ShifterMode::Automatic;
    }
    if (scs_shifter_type == "manual") {
        return ShifterMode::Manual;
    }
    if (scs_shifter_type == "hshifter") {
        return ShifterMode::HShifter;
    }
    return ShifterMode::Unknown;
}

GearRange derive_gear_range(int32_t gear, int32_t displayed_gear) {
    if (displayed_gear != 0) {
        return sign_to_range(displayed_gear);
    }
    return sign_to_range(gear);
}

bool ShiftLockEngine::is_shift_unlocked(const TelemetrySnapshot& t, const PluginConfig& cfg) {
    const float brake = (cfg.brake_source == BrakeSource::Effective) ? t.effective_brake : t.input_brake;
    const float speed_kmh = std::fabs(t.speed_mps) * 3.6f;
    return (brake >= cfg.brake_threshold) && (speed_kmh <= cfg.stop_speed_kmh);
}

void ShiftLockEngine::enqueue_pulses(bool is_up, int count, bool requires_lock, uint64_t now_ms) {
    // Replace any in-flight sequence cleanly so the newest intentional command wins
    queue_.clear();
    if (count <= 0) {
        phase_ = PulsePhase::Idle;
        return;
    }
    active_pulse_ = PendingPulse{is_up, requires_lock};
    phase_ = PulsePhase::Pulsing;
    phase_start_ms_ = now_ms;
    for (int i = 1; i < count; ++i) {
        queue_.push_back(PendingPulse{is_up, requires_lock});
    }
}

void ShiftLockEngine::on_action_pressed(ShiftAction action,
                                        const TelemetrySnapshot& t,
                                        const PluginConfig& cfg,
                                        uint64_t now_ms) {
    if (t.paused || !t.focused) {
        reset();
        return;
    }

    if (t.mode == ShifterMode::Manual) {
        if (action == ShiftAction::ShiftUp) {
            enqueue_pulses(true, 1, false, now_ms);
        } else if (action == ShiftAction::ShiftDown) {
            enqueue_pulses(false, 1, false, now_ms);
        }
        return;
    }

    if (t.mode != ShifterMode::Automatic) {
        return;
    }

    const GearRange current = derive_gear_range(t.gear, t.displayed_gear);
    GearRange target = current;

    switch (action) {
        case ShiftAction::ShiftUp:
            if (current == GearRange::Reverse) {
                target = GearRange::Neutral;
            } else if (current == GearRange::Neutral) {
                target = GearRange::Drive;
            }
            break;
        case ShiftAction::ShiftDown:
            if (current == GearRange::Drive) {
                target = GearRange::Neutral;
            } else if (current == GearRange::Neutral) {
                target = GearRange::Reverse;
            }
            break;
        case ShiftAction::GearD:
            target = GearRange::Drive;
            break;
        case ShiftAction::GearN:
            target = GearRange::Neutral;
            break;
        case ShiftAction::GearR:
            target = GearRange::Reverse;
            break;
        case ShiftAction::Count:
            return;
    }

    const int delta = range_to_int(target) - range_to_int(current);
    if (delta == 0) {
        // Already in requested range (or at end of R..D): silent no-op
        return;
    }

    if (!is_shift_unlocked(t, cfg)) {
        pending_lock_audio_ = true;
        return;
    }

    enqueue_pulses(delta > 0, std::abs(delta), true, now_ms);
}

EngineOutput ShiftLockEngine::tick(const TelemetrySnapshot& t,
                                   const PluginConfig& cfg,
                                   uint64_t now_ms) {
    EngineOutput out;

    if (t.paused || !t.focused || (t.mode != ShifterMode::Automatic && t.mode != ShifterMode::Manual)) {
        reset();
        return out;
    }

    out.trigger_lock_audio = pending_lock_audio_;
    pending_lock_audio_ = false;

    if (phase_ == PulsePhase::Pulsing) {
        if (now_ms >= phase_start_ms_ + cfg.pulse_ms) {
            if (!queue_.empty()) {
                phase_ = PulsePhase::Gap;
                phase_start_ms_ = now_ms;
            } else {
                phase_ = PulsePhase::Idle;
            }
        }
    }

    if (phase_ == PulsePhase::Gap) {
        if (now_ms >= phase_start_ms_ + cfg.pulse_gap_ms) {
            if (!queue_.empty()) {
                const PendingPulse next = queue_.front();
                queue_.pop_front();
                // Re-verify lock conditions before launching subsequent step (e.g. D->N->R)
                if (next.requires_lock && (t.mode != ShifterMode::Automatic || !is_shift_unlocked(t, cfg))) {
                    queue_.clear();
                    phase_ = PulsePhase::Idle;
                } else {
                    active_pulse_ = next;
                    phase_ = PulsePhase::Pulsing;
                    phase_start_ms_ = now_ms;
                }
            } else {
                phase_ = PulsePhase::Idle;
            }
        }
    }

    if (phase_ == PulsePhase::Pulsing) {
        out.gearup_active = active_pulse_.is_up;
        out.geardown_active = !active_pulse_.is_up;
    }

    return out;
}

void ShiftLockEngine::reset() {
    queue_.clear();
    phase_ = PulsePhase::Idle;
    pending_lock_audio_ = false;
}

}  // namespace ets2asl
