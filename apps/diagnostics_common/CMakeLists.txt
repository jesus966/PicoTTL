# PicoTTL diagnostics common infrastructure.
#
# Reusable library code shared by the per-backend CRT diagnostics
# applications (MDA, CGA, future EGA...). Contains the backend-agnostic
# pattern framework (DiagnosticPattern.hpp), the generic pattern
# catalog (Patterns.hpp) and the BOOTSEL button helper. The
# applications themselves remain fully independent of each other: they
# share this library, never each other's code.

add_library(picottl_diagnostics_common STATIC
    BootselButton.cpp
)

target_include_directories(picottl_diagnostics_common PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(picottl_diagnostics_common PUBLIC
    picottl
    pico_stdlib
    hardware_sync
)
