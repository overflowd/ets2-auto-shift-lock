#include "scs_bridge.hpp"

#include "common/scssdk_telemetry_common_configs.h"
#include "common/scssdk_telemetry_truck_common_channels.h"

#include <cstring>

namespace ets2asl {
namespace {

constexpr uint64_t kConfigReloadIntervalMs = 500;
constexpr scs_u32_t kInputIndexGearUp = 0;
constexpr scs_u32_t kInputIndexGearDown = 1;

uint64_t steady_now_ms() {
    return static_cast<uint64_t>(GetTickCount64());
}

}  // namespace

ScsBridge& ScsBridge::instance() {
    static ScsBridge bridge;
    return bridge;
}

void ScsBridge::set_module_handle(HMODULE module_handle) {
    module_handle_ = module_handle;
}

std::filesystem::path ScsBridge::resolve_plugin_dir() const {
    wchar_t path_buf[MAX_PATH] = {};
    if (module_handle_ && GetModuleFileNameW(module_handle_, path_buf, MAX_PATH) > 0) {
        return std::filesystem::path(path_buf).parent_path();
    }
    return std::filesystem::current_path();
}

void ScsBridge::log_msg(scs_log_type_t type, const char* msg) {
    if (log_fn_ && msg) {
        log_fn_(type, msg);
    }
}

void ScsBridge::queue_output_changes_locked(bool gearup, bool geardown) {
    if (gearup != last_sent_gearup_) {
        last_sent_gearup_ = gearup;
        if (input_device_active_) {
            pending_input_events_.push_back(PendingInputEvent{kInputIndexGearUp, gearup});
        }
    }
    if (geardown != last_sent_geardown_) {
        last_sent_geardown_ = geardown;
        if (input_device_active_) {
            pending_input_events_.push_back(PendingInputEvent{kInputIndexGearDown, geardown});
        }
    }
}

scs_result_t ScsBridge::init_telemetry(scs_u32_t version, const scs_telemetry_init_params_t* params) {
    if (version != SCS_TELEMETRY_VERSION_1_01 && version != SCS_TELEMETRY_VERSION_1_00) {
        return SCS_RESULT_unsupported;
    }
    if (!params) {
        return SCS_RESULT_invalid_parameter;
    }

    const auto* vp = static_cast<const scs_telemetry_init_params_v100_t*>(params);

    std::lock_guard<std::mutex> lock(mutex_);
    log_fn_ = vp->common.log;

    const std::filesystem::path plugin_dir = resolve_plugin_dir();
    config_watcher_ = std::make_unique<ConfigWatcher>(plugin_dir / "ets2_auto_shift_lock.ini");
    audio_alert_ = std::make_unique<AudioAlert>(plugin_dir);
    config_ = PluginConfig{};
    config_watcher_->reload_if_changed(config_);
    last_config_check_ms_ = steady_now_ms();

    snapshot_ = TelemetrySnapshot{};
    snapshot_.paused = true;
    engine_.reset();
    input_poller_.initialize(module_handle_);

    if (vp->register_for_event(SCS_TELEMETRY_EVENT_configuration, &ScsBridge::on_configuration, this) != SCS_RESULT_ok ||
        vp->register_for_event(SCS_TELEMETRY_EVENT_paused, &ScsBridge::on_paused_or_started, this) != SCS_RESULT_ok ||
        vp->register_for_event(SCS_TELEMETRY_EVENT_started, &ScsBridge::on_paused_or_started, this) != SCS_RESULT_ok ||
        vp->register_for_event(SCS_TELEMETRY_EVENT_frame_end, &ScsBridge::on_frame_end, this) != SCS_RESULT_ok) {
        log_msg(SCS_LOG_TYPE_error, "[AutoShiftLock] Failed to register telemetry events.");
        return SCS_RESULT_generic_error;
    }

    vp->register_for_channel(
        SCS_TELEMETRY_TRUCK_CHANNEL_speed, SCS_U32_NIL, SCS_VALUE_TYPE_float,
        SCS_TELEMETRY_CHANNEL_FLAG_none, &ScsBridge::on_channel_float, &snapshot_.speed_mps);
    vp->register_for_channel(
        SCS_TELEMETRY_TRUCK_CHANNEL_input_brake, SCS_U32_NIL, SCS_VALUE_TYPE_float,
        SCS_TELEMETRY_CHANNEL_FLAG_none, &ScsBridge::on_channel_float, &snapshot_.input_brake);
    vp->register_for_channel(
        SCS_TELEMETRY_TRUCK_CHANNEL_effective_brake, SCS_U32_NIL, SCS_VALUE_TYPE_float,
        SCS_TELEMETRY_CHANNEL_FLAG_none, &ScsBridge::on_channel_float, &snapshot_.effective_brake);
    vp->register_for_channel(
        SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear, SCS_U32_NIL, SCS_VALUE_TYPE_s32,
        SCS_TELEMETRY_CHANNEL_FLAG_none, &ScsBridge::on_channel_s32, &snapshot_.gear);
    vp->register_for_channel(
        SCS_TELEMETRY_TRUCK_CHANNEL_displayed_gear, SCS_U32_NIL, SCS_VALUE_TYPE_s32,
        SCS_TELEMETRY_CHANNEL_FLAG_none, &ScsBridge::on_channel_s32, &snapshot_.displayed_gear);

    log_msg(SCS_LOG_TYPE_message, "[AutoShiftLock] Telemetry initialized.");
    return SCS_RESULT_ok;
}

void ScsBridge::shutdown_telemetry() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_output_changes_locked(false, false);
    engine_.reset();
    input_poller_.shutdown();
    audio_alert_.reset();
    config_watcher_.reset();
    log_msg(SCS_LOG_TYPE_message, "[AutoShiftLock] Telemetry shut down.");
}

scs_result_t ScsBridge::init_input(scs_u32_t version, const scs_input_init_params_t* params) {
    if (version != SCS_INPUT_VERSION_1_00) {
        return SCS_RESULT_unsupported;
    }
    if (!params) {
        return SCS_RESULT_invalid_parameter;
    }

    const auto* vp = static_cast<const scs_input_init_params_v100_t*>(params);

    std::lock_guard<std::mutex> lock(mutex_);
    if (!log_fn_) {
        log_fn_ = vp->common.log;
    }

    scs_input_device_input_t inputs[2];
    std::memset(inputs, 0, sizeof(inputs));
    inputs[kInputIndexGearUp].name = "gearup";
    inputs[kInputIndexGearUp].display_name = "Shift Up";
    inputs[kInputIndexGearUp].value_type = SCS_VALUE_TYPE_bool;

    inputs[kInputIndexGearDown].name = "geardown";
    inputs[kInputIndexGearDown].display_name = "Shift Down";
    inputs[kInputIndexGearDown].value_type = SCS_VALUE_TYPE_bool;

    scs_input_device_t device_info;
    std::memset(&device_info, 0, sizeof(device_info));
    device_info.name = "auto_shift_lock";
    device_info.display_name = "Auto Shift Lock";
    device_info.type = SCS_INPUT_DEVICE_TYPE_semantical;
    device_info.input_count = 2;
    device_info.inputs = inputs;
    device_info.callback_context = this;
    device_info.input_active_callback = &ScsBridge::on_input_active;
    device_info.input_event_callback = &ScsBridge::on_input_event;

    if (vp->register_device(&device_info) != SCS_RESULT_ok) {
        log_msg(SCS_LOG_TYPE_error, "[AutoShiftLock] Failed to register semantical input device.");
        return SCS_RESULT_generic_error;
    }

    log_msg(SCS_LOG_TYPE_message, "[AutoShiftLock] Semantical input device registered (gearup, geardown).");
    return SCS_RESULT_ok;
}

void ScsBridge::shutdown_input() {
    std::lock_guard<std::mutex> lock(mutex_);
    input_device_active_ = false;
    pending_input_events_.clear();
    last_sent_gearup_ = false;
    last_sent_geardown_ = false;
    log_msg(SCS_LOG_TYPE_message, "[AutoShiftLock] Input device shut down.");
}

SCSAPI_VOID ScsBridge::on_configuration(scs_event_t, const void* event_info, scs_context_t context) {
    auto* self = static_cast<ScsBridge*>(context);
    const auto* info = static_cast<const scs_telemetry_configuration_t*>(event_info);
    if (!self || !info || !info->id || !info->attributes) {
        return;
    }
    if (std::strcmp(info->id, SCS_TELEMETRY_CONFIG_controls) != 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(self->mutex_);
    for (const scs_named_value_t* attr = info->attributes; attr->name != nullptr; ++attr) {
        if (std::strcmp(attr->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_shifter_type) == 0 &&
            attr->value.type == SCS_VALUE_TYPE_string &&
            attr->value.value_string.value != nullptr) {
            self->snapshot_.mode = parse_shifter_mode(attr->value.value_string.value);
            break;
        }
    }
}

SCSAPI_VOID ScsBridge::on_paused_or_started(scs_event_t event, const void*, scs_context_t context) {
    auto* self = static_cast<ScsBridge*>(context);
    if (!self) {
        return;
    }
    std::lock_guard<std::mutex> lock(self->mutex_);
    const bool now_paused = (event == SCS_TELEMETRY_EVENT_paused);
    self->snapshot_.paused = now_paused;
    if (now_paused) {
        self->engine_.reset();
        self->input_poller_.reset_edges();
        self->queue_output_changes_locked(false, false);
    }
}

SCSAPI_VOID ScsBridge::on_frame_end(scs_event_t, const void*, scs_context_t context) {
    auto* self = static_cast<ScsBridge*>(context);
    if (!self) {
        return;
    }

    const uint64_t now_ms = steady_now_ms();
    std::lock_guard<std::mutex> lock(self->mutex_);

    if (self->config_watcher_ && now_ms >= self->last_config_check_ms_ + kConfigReloadIntervalMs) {
        self->last_config_check_ms_ = now_ms;
        if (self->config_watcher_->reload_if_changed(self->config_)) {
            self->input_poller_.reset_edges();
            self->log_msg(SCS_LOG_TYPE_message, "[AutoShiftLock] Reloaded ets2_auto_shift_lock.ini.");
        }
    }

    const bool allow_capture = !self->snapshot_.paused &&
                               (self->snapshot_.mode == ShifterMode::Automatic ||
                                self->snapshot_.mode == ShifterMode::Manual);
    const auto edges = self->input_poller_.poll_pressed_edges(self->config_, allow_capture, now_ms);

    for (size_t i = 0; i < kShiftActionCount; ++i) {
        if (edges[i]) {
            self->engine_.on_action_pressed(
                static_cast<ShiftAction>(i), self->snapshot_, self->config_, now_ms);
        }
    }

    const EngineOutput out = self->engine_.tick(self->snapshot_, self->config_, now_ms);
    self->queue_output_changes_locked(out.gearup_active, out.geardown_active);

    if (out.trigger_lock_audio && self->audio_alert_) {
        self->audio_alert_->try_play(self->config_, now_ms);
    }
}

SCSAPI_VOID ScsBridge::on_channel_float(scs_string_t, scs_u32_t, const scs_value_t* value, scs_context_t context) {
    auto* target = static_cast<float*>(context);
    if (!target) {
        return;
    }
    *target = (value && value->type == SCS_VALUE_TYPE_float) ? value->value_float.value : 0.0f;
}

SCSAPI_VOID ScsBridge::on_channel_s32(scs_string_t, scs_u32_t, const scs_value_t* value, scs_context_t context) {
    auto* target = static_cast<int32_t*>(context);
    if (!target) {
        return;
    }
    *target = (value && value->type == SCS_VALUE_TYPE_s32) ? value->value_s32.value : 0;
}

SCSAPI_VOID ScsBridge::on_input_active(scs_u8_t active, scs_context_t context) {
    auto* self = static_cast<ScsBridge*>(context);
    if (!self) {
        return;
    }
    std::lock_guard<std::mutex> lock(self->mutex_);
    self->input_device_active_ = (active != 0);
    self->pending_input_events_.clear();
    self->last_sent_gearup_ = false;
    self->last_sent_geardown_ = false;
}

SCSAPI_RESULT ScsBridge::on_input_event(scs_input_event_t* event_info, scs_u32_t flags, scs_context_t context) {
    auto* self = static_cast<ScsBridge*>(context);
    if (!self || !event_info) {
        return SCS_RESULT_not_found;
    }

    std::lock_guard<std::mutex> lock(self->mutex_);
    if ((flags & SCS_INPUT_EVENT_CALLBACK_FLAG_first_after_activation) != 0) {
        self->pending_input_events_.clear();
        self->pending_input_events_.push_back(PendingInputEvent{kInputIndexGearUp, self->last_sent_gearup_});
        self->pending_input_events_.push_back(PendingInputEvent{kInputIndexGearDown, self->last_sent_geardown_});
    }

    if (self->pending_input_events_.empty()) {
        return SCS_RESULT_not_found;
    }

    const PendingInputEvent ev = self->pending_input_events_.front();
    self->pending_input_events_.pop_front();
    event_info->input_index = ev.input_index;
    event_info->value_bool.value = ev.value ? 1 : 0;
    return SCS_RESULT_ok;
}

}  // namespace ets2asl
