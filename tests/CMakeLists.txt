# PicoTTL host test suite.
#
# A standalone desktop build: compiles the platform-independent framework
# sources plus the host backend and runs the automated regression tests.
# RP2350 devices use a recording engine. The real engine's control flow is
# also compiled under a distinct name against a recording SDK boundary.
# No real Pico SDK code, MMIO, PIO execution or DMA transfers run on host.
# The private DMA stop-order helper is exercised with recording callbacks.
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
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Host-only instrumentation. These options never affect the firmware target.
option(PICOTTL_ENABLE_SANITIZERS "Enable host Address/UndefinedBehaviorSanitizer" OFF)
option(PICOTTL_ENABLE_COVERAGE "Enable host gcov instrumentation" OFF)
if(PICOTTL_ENABLE_SANITIZERS OR PICOTTL_ENABLE_COVERAGE)
    if(CMAKE_CROSSCOMPILING OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        message(FATAL_ERROR "Host instrumentation requires a native GCC or Clang build")
    endif()
endif()

# Track the canonical implementation as a configure input. Copying it avoids
# a .cpp include while allowing an isolated SDK boundary and test-only name.
set(PICOTTL_HOST_ENGINE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/HardwareScanlineEngine.cpp")
configure_file(${CMAKE_CURRENT_LIST_DIR}/../src/rp2350/PioScanlineEngine.cpp
    ${PICOTTL_HOST_ENGINE_SOURCE} COPYONLY)

# A separately named build of the same engine exercises the RP2350B
# 48-GPIO compile-time limit in the same CI/sanitizer executable.
set(PICOTTL_HOST_B_ENGINE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/HighGpioScanlineEngine.cpp")
configure_file(${CMAKE_CURRENT_LIST_DIR}/../src/rp2350/PioScanlineEngine.cpp
    ${PICOTTL_HOST_B_ENGINE_SOURCE} COPYONLY)

add_executable(picottl_tests
    framework/test.cpp
    test_packed_framebuffer.cpp
    test_graphics.cpp
    test_text.cpp
    test_code_page.cpp
    test_terminal.cpp
    test_terminal_sizing.cpp
    test_display_lifecycle.cpp
    test_host_image_protocol.cpp
    test_ansi_terminal.cpp
    test_framebuffer_lifecycle.cpp
    test_dma_stop.cpp
    test_video_timing.cpp
    test_engine_gpio.cpp
    ${PICOTTL_HOST_B_ENGINE_SOURCE}
    test_engine_configuration.cpp
    ${PICOTTL_HOST_ENGINE_SOURCE}
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

# Only the host-compiled engine sees these SDK stand-ins. Portable sources
# and the existing device recording engine keep their normal include paths.
set_source_files_properties(${PICOTTL_HOST_ENGINE_SOURCE} PROPERTIES
    COMPILE_DEFINITIONS "PioScanlineEngine=HardwareScanlineEngine"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/support/pico_sdk;${CMAKE_CURRENT_LIST_DIR}/../src/rp2350")

set_source_files_properties(${PICOTTL_HOST_B_ENGINE_SOURCE} PROPERTIES
    COMPILE_DEFINITIONS "PioScanlineEngine=HighGpioScanlineEngine;PICOTTL_TEST_GPIO_COUNT=48"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/support/pico_sdk;${CMAKE_CURRENT_LIST_DIR}/../src/rp2350")
set_source_files_properties(test_engine_gpio.cpp PROPERTIES
    COMPILE_DEFINITIONS "PICOTTL_TEST_GPIO_COUNT=48")

target_include_directories(picottl_tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

if(MSVC)
    target_compile_options(picottl_tests PRIVATE /W4)
else()
    target_compile_options(picottl_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(PICOTTL_ENABLE_SANITIZERS)
    target_compile_options(picottl_tests PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer
        -fno-sanitize-recover=all)
    target_link_options(picottl_tests PRIVATE -fsanitize=address,undefined)
endif()

if(PICOTTL_ENABLE_COVERAGE)
    target_compile_options(picottl_tests PRIVATE --coverage -O0 -g)
    target_link_options(picottl_tests PRIVATE --coverage)
endif()

enable_testing()
add_test(NAME picottl_host_tests COMMAND picottl_tests)
set_tests_properties(picottl_host_tests PROPERTIES TIMEOUT 30)
