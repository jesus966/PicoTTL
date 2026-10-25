# PicoTTL host test suite.
#
# A standalone desktop build: compiles the platform-independent framework
# sources plus the host backend and runs the automated regression tests.
# RP2350 device lifecycle code is tested with a recording engine; the real
# scanline engine and all Pico SDK hardware code are excluded from this build.
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
    test_framebuffer_lifecycle.cpp
    # Real backend lifecycle code, linked to a recording engine instead
    # of the hardware implementation. This does not emulate PIO or DMA.
    support/RecordingScanlineEngine.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/rp2350/MdaDisplayDevice.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/rp2350/CgaDisplayDevice.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/rp2350/EgaDisplayDevice.cpp
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
