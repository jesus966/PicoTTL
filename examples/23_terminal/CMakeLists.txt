# PicoTTL Example 23: terminal (cell buffer, blink, overlay cursor, auto-scroll).

add_executable(picottl_example_23_terminal
    main.cpp
)

pico_set_program_name(picottl_example_23_terminal "PicoTTL Example 23 - Terminal")
pico_set_program_version(picottl_example_23_terminal "0.1")

target_link_libraries(picottl_example_23_terminal PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_23_terminal)
