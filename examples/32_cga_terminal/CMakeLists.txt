# PicoTTL Example 32: CGA terminal (color attributes, cursor, blink, scroll).

add_executable(picottl_example_32_cga_terminal
    main.cpp
)

pico_set_program_name(picottl_example_32_cga_terminal "PicoTTL Example 32 - CGA Terminal")
pico_set_program_version(picottl_example_32_cga_terminal "0.1")

target_link_libraries(picottl_example_32_cga_terminal PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_32_cga_terminal)
