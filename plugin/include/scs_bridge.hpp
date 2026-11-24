#pragma once

#include "audio_alert.hpp"
#include "config_store.hpp"
#include "input_poller.hpp"
#include "shift_lock_engine.hpp"

#include "scssdk_input.h"
#include "scssdk_telemetry.h"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>

namespace ets2asl {

class ScsBridge {
public:
    static ScsBridge& instance();

    void set_module_handle(HMODULE module_handle);

    scs_result_t init_telemetry(scs_u32_t version, const scs_telemetry_init_params_t* params);
    void shutdown_telemetry();

    scs_result_t init_input(scs_u32_t version, const scs_input_init_params_t* params);
    void shutdown_input();

private:
    struct PendingInputEvent {
        scs_u32_t input_index = 0;
        bool value = false;
    };

    ScsBridge() = default;

    std::filesystem::path resolve_plugin_dir() const;
    void log_msg(scs_log_type_t type, const char* msg);
    void queue_output_changes_locked(bool gearup, bool geardown);

    static SCSAPI_VOID on_configuration(scs_event_t event, const void* event_info, scs_context_t context);
    static SCSAPI_VOID on_paused_or_started(scs_event_t event, const void* event_info, scs_context_t context);
    static SCSAPI_VOID on_frame_end(scs_event_t event, const void* event_info, scs_context_t context);

    static SCSAPI_VOID on_channel_float(scs_string_t name, scs_u32_t index, const scs_value_t* value, scs_context_t context);
    static SCSAPI_VOID on_channel_s32(scs_string_t name, scs_u32_t index, const scs_value_t* value, scs_context_t context);

    static SCSAPI_VOID on_input_active(scs_u8_t active, scs_context_t context);
    static SCSAPI_RESULT on_input_event(scs_input_event_t* event_info, scs_u32_t flags, scs_context_t context);

    HMODULE module_handle_ = nullptr;
    scs_log_t log_fn_ = nullptr;

    std::mutex mutex_;
    TelemetrySnapshot snapshot_{};
    PluginConfig config_{};
    std::unique_ptr<ConfigWatcher> config_watcher_;
    std::unique_ptr<AudioAlert> audio_alert_;
    InputPoller input_poller_;
    ShiftLockEngine engine_;

    uint64_t last_config_check_ms_ = 0;
    bool input_device_active_ = false;
    bool last_sent_gearup_ = false;
    bool last_sent_geardown_ = false;
    std::deque<PendingInputEvent> pending_input_events_;
};

}  // namespace ets2asl
