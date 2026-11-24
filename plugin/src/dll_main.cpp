#include "scs_bridge.hpp"

extern "C" {

SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t* const params) {
    return ets2asl::ScsBridge::instance().init_telemetry(version, params);
}

SCSAPI_VOID scs_telemetry_shutdown(void) {
    ets2asl::ScsBridge::instance().shutdown_telemetry();
}

SCSAPI_RESULT scs_input_init(const scs_u32_t version, const scs_input_init_params_t* const params) {
    return ets2asl::ScsBridge::instance().init_input(version, params);
}

SCSAPI_VOID scs_input_shutdown(void) {
    ets2asl::ScsBridge::instance().shutdown_input();
}

}  // extern "C"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        ets2asl::ScsBridge::instance().set_module_handle(module);
    }
    return TRUE;
}
