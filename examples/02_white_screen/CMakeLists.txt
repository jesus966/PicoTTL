# PicoTTL Example 02: white screen (framebuffer scan-out).

add_executable(picottl_example_02_white_screen
    main.cpp
)

pico_set_program_name(picottl_example_02_white_screen "PicoTTL Example 02 - White Screen")
pico_set_program_version(picottl_example_02_white_screen "0.1")

target_link_libraries(picottl_example_02_white_screen PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_02_white_screen)
