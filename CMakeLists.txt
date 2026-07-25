# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

# Equivalent to the SDK's pico_is_top_level_project(), but needed before
# the SDK is loaded.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(PICOTTL_TOP_LEVEL_PROJECT ON)
else()
    set(PICOTTL_TOP_LEVEL_PROJECT OFF)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(PICOTTL_TOP_LEVEL_PROJECT)
# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(PicoTTL C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()
else()
    if(NOT TARGET pico_stdlib)
        message(FATAL_ERROR
            "PicoTTL must be added after pico_sdk_init() in the parent project")
    endif()
    if(NOT PICO_PLATFORM MATCHES "^rp2350")
        message(FATAL_ERROR "PicoTTL requires an RP2350 target platform")
    endif()
endif()

# ---------------------------------------------------------------------------
# PicoTTL framework library
# ---------------------------------------------------------------------------

add_library(picottl STATIC
    # Platform-independent modules
    src/AnsiTerminal.cpp
    src/DisplaySurface.cpp
    src/Graphics.cpp
    src/Terminal.cpp
    src/Text.cpp
    src/fonts/IbmCga8x8.cpp
    src/fonts/IbmCga8x8Cp850.cpp
    src/fonts/IbmEga8x14.cpp
    src/fonts/IbmEga8x14Cp850.cpp
    src/fonts/IbmMda9x14.cpp
    src/fonts/IbmMda9x14Cp850.cpp
    # RP2350 backend (the only Pico SDK dependent code)
    src/rp2350/CgaDisplayDevice.cpp
    src/rp2350/EgaDisplayDevice.cpp
    src/rp2350/MdaDisplayDevice.cpp
    src/rp2350/PioScanlineEngine.cpp
)

target_include_directories(picottl PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_features(picottl PUBLIC cxx_std_17)

target_link_libraries(picottl PUBLIC
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_clocks
)

pico_generate_pio_header(picottl
    ${CMAKE_CURRENT_LIST_DIR}/src/rp2350/ttl_timing.pio
)

pico_generate_pio_header(picottl
    ${CMAKE_CURRENT_LIST_DIR}/src/rp2350/ttl_pixels.pio
)

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------

if(PICOTTL_TOP_LEVEL_PROJECT)
add_subdirectory(examples)

# ---------------------------------------------------------------------------
# CRT diagnostics tools: shared pattern infrastructure + one permanent
# validation application per backend family.
# ---------------------------------------------------------------------------

add_subdirectory(apps/diagnostics_common)
add_subdirectory(apps/mda_crt_diagnostics) # MDA CRT diagnostics
add_subdirectory(apps/cga_crt_diagnostics) # CGA CRT diagnostics
add_subdirectory(apps/ega_crt_diagnostics) # EGA CRT diagnostics
endif()

