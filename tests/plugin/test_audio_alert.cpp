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

TEST_CASE(audio_alert_resolves_utf8_turkish_wav_path) {
    const auto tmp_dir = std::filesystem::temp_directory_path() / L"ets2asl_t\u00FCrk\u00E7e_\u015F\u011F";
    std::error_code ec;
    std::filesystem::create_directories(tmp_dir, ec);
    const auto wav_path = tmp_dir / L"kilit_uyar\u0131s\u0131_\u00E7\u011F\u0131\u00F6\u015F\u00FC.wav";
    {
        std::ofstream f(wav_path, std::ios::binary);
        f << "RIFF";
    }

    // UTF-8 encoded filename as written by PluginConfig.Save in C#
    const std::string utf8_name =
        "kilit_uyar\xC4\xB1s\xC4\xB1_\xC3\xA7\xC4\x9F\xC4\xB1\xC3\xB6\xC5\x9F\xC3\xBC.wav";

    AudioAlert alert(tmp_dir, /*dry_run=*/true);
    PluginConfig cfg;
    cfg.audio_enabled = true;
    cfg.audio_sound_file = utf8_name;

    CHECK(alert.try_play(cfg, 1000) == AlertBackendResult::PlayedWav);

    std::filesystem::remove_all(tmp_dir, ec);
}
