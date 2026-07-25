# PicoTTL Example 03: single horizontal line.

add_executable(picottl_example_03_horizontal_line
    main.cpp
)

pico_set_program_name(picottl_example_03_horizontal_line "PicoTTL Example 03 - Horizontal Line")
pico_set_program_version(picottl_example_03_horizontal_line "0.1")

target_link_libraries(picottl_example_03_horizontal_line PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_03_horizontal_line)
