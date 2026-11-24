#include "test_framework.hpp"

#include "audio_alert.hpp"

#include <filesystem>
#include <fstream>

using namespace ets2asl;

TEST_CASE(audio_alert_respects_enabled_flag_and_cooldown) {
    const auto tmp_dir = std::filesystem::temp_directory_path() / "ets2asl_audio_test";
    std::error_code ec;
    std::filesystem::create_directories(tmp_dir, ec);
    const auto wav_path = tmp_dir / "lock_warning.wav";
    {
        std::ofstream f(wav_path, std::ios::binary);
        f << "RIFF";
    }

    AudioAlert alert(tmp_dir, /*dry_run=*/true);
    PluginConfig cfg;
    cfg.audio_cooldown_ms = 350;
    cfg.audio_sound_file = "lock_warning.wav";

    cfg.audio_enabled = false;
    CHECK(alert.try_play(cfg, 1000) == AlertBackendResult::SkippedDisabled);

    cfg.audio_enabled = true;
    CHECK(alert.try_play(cfg, 1000) == AlertBackendResult::PlayedWav);
    CHECK(alert.try_play(cfg, 1200) == AlertBackendResult::SkippedCooldown);
    CHECK(alert.try_play(cfg, 1349) == AlertBackendResult::SkippedCooldown);
    CHECK(alert.try_play(cfg, 1350) == AlertBackendResult::PlayedWav);

    alert.reset_cooldown();
    CHECK(alert.try_play(cfg, 1360) == AlertBackendResult::PlayedWav);

    std::filesystem::remove_all(tmp_dir, ec);
}

TEST_CASE(audio_alert_falls_back_when_wav_missing) {
    // Review Focus #5
    const auto tmp_dir = std::filesystem::temp_directory_path() / "ets2asl_audio_missing";
    AudioAlert alert(tmp_dir, /*dry_run=*/true);
    PluginConfig cfg;
    cfg.audio_enabled = true;
    cfg.audio_sound_file = "nonexistent_lock_file.wav";

    CHECK(alert.try_play(cfg, 5000) == AlertBackendResult::PlayedFallbackBeep);
    CHECK(alert.try_play(cfg, 5100) == AlertBackendResult::SkippedCooldown);
}
