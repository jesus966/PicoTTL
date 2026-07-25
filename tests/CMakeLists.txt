# PicoTTL host test suite.
#
# A standalone desktop build: compiles the platform-independent framework
# sources plus the host backend and runs the automated regression tests.
# No Pico SDK, no external libraries - plain C++17.
#
# Usage:
#   cmake -S tests -B build-host -G Ninja
#   cmake --build build-host
#   ./build-host/picottl_tests

cmake_minimum_required(VERSION 3.13)

project(picottl_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(picottl_tests
    framework/test.cpp
    test_packed_framebuffer.cpp
    test_graphics.cpp
    test_text.cpp
    test_code_page.cpp
    test_terminal.cpp
    test_ansi_terminal.cpp
    # Platform-independent framework sources (no RP2350 code here).
    ${CMAKE_CURRENT_LIST_DIR}/../src/AnsiTerminal.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/DisplaySurface.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/Graphics.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/Terminal.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/Text.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmCga8x8.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmCga8x8Cp850.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmEga8x14.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmEga8x14Cp850.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmMda9x14.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmMda9x14Cp850.cpp
    # Host backend.
    ${CMAKE_CURRENT_LIST_DIR}/../src/host/HostDisplayDevice.cpp
)

target_include_directories(picottl_tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

if(MSVC)
    target_compile_options(picottl_tests PRIVATE /W4)
else()
    target_compile_options(picottl_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
