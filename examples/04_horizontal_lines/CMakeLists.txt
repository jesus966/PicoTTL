# PicoTTL Example 04: several horizontal lines.

add_executable(picottl_example_04_horizontal_lines
    main.cpp
)

pico_set_program_name(picottl_example_04_horizontal_lines "PicoTTL Example 04 - Horizontal Lines")
pico_set_program_version(picottl_example_04_horizontal_lines "0.1")

target_link_libraries(picottl_example_04_horizontal_lines PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_04_horizontal_lines)
