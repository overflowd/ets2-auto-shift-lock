#include "audio_alert.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>

namespace ets2asl {

AudioAlert::AudioAlert(std::filesystem::path plugin_dir, bool dry_run)
    : plugin_dir_(std::move(plugin_dir)), dry_run_(dry_run) {}

std::filesystem::path AudioAlert::resolve_wav_path(const std::string& sound_file) const {
    if (sound_file.empty()) {
        return {};
    }
    const int wide_len = MultiByteToWideChar(
        CP_UTF8, 0, sound_file.data(), static_cast<int>(sound_file.size()), nullptr, 0);
    std::wstring wide_name;
    if (wide_len > 0) {
        wide_name.resize(static_cast<size_t>(wide_len));
        MultiByteToWideChar(
            CP_UTF8, 0, sound_file.data(), static_cast<int>(sound_file.size()),
            wide_name.data(), wide_len);
    } else {
        wide_name.assign(sound_file.begin(), sound_file.end());
    }

    std::filesystem::path p(wide_name);
    if (p.is_absolute()) {
        return p;
    }
    return plugin_dir_ / p;
}

AlertBackendResult AudioAlert::try_play(const PluginConfig& cfg, uint64_t now_ms) {
    if (!cfg.audio_enabled) {
        return AlertBackendResult::SkippedDisabled;
    }

    if (has_played_ && now_ms < last_play_ms_ + cfg.audio_cooldown_ms) {
        return AlertBackendResult::SkippedCooldown;
    }

    has_played_ = true;
    last_play_ms_ = now_ms;

    const std::filesystem::path wav_path = resolve_wav_path(cfg.audio_sound_file);
    std::error_code ec;
    const bool wav_exists = !wav_path.empty() && std::filesystem::is_regular_file(wav_path, ec) && !ec;

    if (wav_exists) {
        if (dry_run_) {
            return AlertBackendResult::PlayedWav;
        }
        const std::wstring wide_path = wav_path.wstring();
        if (PlaySoundW(wide_path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) {
            return AlertBackendResult::PlayedWav;
        }
    }

    if (!dry_run_) {
        MessageBeep(MB_ICONWARNING);
    }
    return AlertBackendResult::PlayedFallbackBeep;
}

void AudioAlert::reset_cooldown() {
    has_played_ = false;
    last_play_ms_ = 0;
}

}  // namespace ets2asl
