cmake_minimum_required(VERSION 3.20)
project(ets2_auto_shift_lock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ets2_auto_shift_lock must be built for x64 (64-bit) Windows.")
endif()

enable_testing()

add_executable(plugin_tests
    tests/plugin/test_main.cpp
)
target_include_directories(plugin_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/plugin
)

if(MINGW)
    target_link_options(plugin_tests PRIVATE -static-libgcc -static-libstdc++)
endif()

add_test(NAME plugin_tests COMMAND plugin_tests)
