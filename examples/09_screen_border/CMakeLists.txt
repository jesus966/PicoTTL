# PicoTTL Example 09: screen border (visible-area geometry validation).

add_executable(picottl_example_09_screen_border
    main.cpp
)

pico_set_program_name(picottl_example_09_screen_border "PicoTTL Example 09 - Screen Border")
pico_set_program_version(picottl_example_09_screen_border "0.1")

target_link_libraries(picottl_example_09_screen_border PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_09_screen_border)
