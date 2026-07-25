# PicoTTL Example 41: EGA terminal (default palette, cursor, blink, scroll).

add_executable(picottl_example_41_ega_terminal
    main.cpp
)

pico_set_program_name(picottl_example_41_ega_terminal "PicoTTL Example 41 - EGA Terminal")
pico_set_program_version(picottl_example_41_ega_terminal "0.1")

target_link_libraries(picottl_example_41_ega_terminal PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_41_ega_terminal)
