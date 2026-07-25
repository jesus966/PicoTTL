# PicoTTL showcase demonstrations - shared utility library.
#
# Drawing helpers (filled shapes, ellipses, ordered dithering, era-authentic
# transitions) and text-mode helpers (CP437 boxes, typewriter output, scaled
# banner lettering, the shared technical-information and closing screens)
# used by the three official demonstration programs (examples 96-98).
#
# Application-level code: like apps/diagnostics_common, this library may use
# the Pico SDK directly (sleep_ms pacing) - it is shared APP code, not part
# of the PicoTTL framework.

add_library(picottl_demo_common STATIC
    DemoDraw.cpp
    DemoText.cpp
)

target_include_directories(picottl_demo_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(picottl_demo_common PUBLIC
    picottl
    pico_stdlib
)
