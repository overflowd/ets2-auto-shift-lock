cmake_minimum_required(VERSION 3.20)
project(ets2_auto_shift_lock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ets2_auto_shift_lock must be built for x64 (64-bit) Windows.")
endif()

enable_testing()

add_library(ets2_auto_shift_lock SHARED
    plugin/src/config_store.cpp
    plugin/src/shift_lock_engine.cpp
    plugin/src/audio_alert.cpp
    plugin/src/input_poller.cpp
    plugin/src/scs_bridge.cpp
    plugin/src/dll_main.cpp
    plugin/exports.def
)
set_target_properties(ets2_auto_shift_lock PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ets2_auto_shift_lock"
)
target_include_directories(ets2_auto_shift_lock PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/scs_sdk/include
)
target_link_libraries(ets2_auto_shift_lock PRIVATE winmm user32 dinput8 dxguid)

if(MINGW)
    target_link_options(ets2_auto_shift_lock PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
endif()

add_executable(plugin_tests
    plugin/src/config_store.cpp
    plugin/src/shift_lock_engine.cpp
    plugin/src/audio_alert.cpp
    plugin/src/input_poller.cpp
    tests/plugin/test_main.cpp
    tests/plugin/test_config_store.cpp
    tests/plugin/test_shift_lock_engine.cpp
    tests/plugin/test_audio_alert.cpp
    tests/plugin/test_input_poller.cpp
)
target_include_directories(plugin_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/plugin
)
target_link_libraries(plugin_tests PRIVATE winmm user32 dinput8 dxguid)

if(MINGW)
    target_link_options(plugin_tests PRIVATE -static-libgcc -static-libstdc++)
endif()

add_test(NAME plugin_tests COMMAND plugin_tests)
