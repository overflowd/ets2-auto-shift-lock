#pragma once

#include "binding_types.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace ets2asl {

struct PluginConfig {
    float brake_threshold = 0.25f;
    float stop_speed_kmh = 0.5f;
    BrakeSource brake_source = BrakeSource::Input;

    bool audio_enabled = true;
    std::string audio_sound_file = "lock_warning.wav";
    uint32_t audio_cooldown_ms = 350;

    uint32_t pulse_ms = 60;
    uint32_t pulse_gap_ms = 80;

    std::array<BindingSpec, kShiftActionCount> bindings{};
};

BindingSpec parse_binding_token(std::string_view token);
std::string format_binding_token(const BindingSpec& spec);

PluginConfig parse_ini_text(std::string_view text);
std::string serialize_ini_text(const PluginConfig& cfg);

class ConfigWatcher {
public:
    explicit ConfigWatcher(std::filesystem::path ini_path);

    bool reload_if_changed(PluginConfig& out_cfg);
    const std::filesystem::path& path() const { return ini_path_; }

private:
    std::filesystem::path ini_path_;
    bool has_loaded_ = false;
    std::filesystem::file_time_type last_mtime_{};
};

}  // namespace ets2asl
