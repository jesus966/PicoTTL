# PicoTTL Example 05: single vertical line.

add_executable(picottl_example_05_vertical_line
    main.cpp
)

pico_set_program_name(picottl_example_05_vertical_line "PicoTTL Example 05 - Vertical Line")
pico_set_program_version(picottl_example_05_vertical_line "0.1")

target_link_libraries(picottl_example_05_vertical_line PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_05_vertical_line)
