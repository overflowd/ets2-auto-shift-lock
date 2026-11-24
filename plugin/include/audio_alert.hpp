#pragma once

#include "config_store.hpp"

#include <cstdint>
#include <filesystem>

namespace ets2asl {

enum class AlertBackendResult : uint8_t {
    SkippedDisabled = 0,
    SkippedCooldown,
    PlayedWav,
    PlayedFallbackBeep,
};

class AudioAlert {
public:
    explicit AudioAlert(std::filesystem::path plugin_dir, bool dry_run = false);

    AlertBackendResult try_play(const PluginConfig& cfg, uint64_t now_ms);
    void reset_cooldown();

private:
    std::filesystem::path resolve_wav_path(const std::string& sound_file) const;

    std::filesystem::path plugin_dir_;
    bool dry_run_ = false;
    bool has_played_ = false;
    uint64_t last_play_ms_ = 0;
};

}  // namespace ets2asl
