# PicoTTL Example 07: pixel checkerboard (definitive pixel-path validation).

add_executable(picottl_example_07_checkerboard
    main.cpp
)

pico_set_program_name(picottl_example_07_checkerboard "PicoTTL Example 07 - Checkerboard")
pico_set_program_version(picottl_example_07_checkerboard "0.1")

target_link_libraries(picottl_example_07_checkerboard PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_07_checkerboard)
