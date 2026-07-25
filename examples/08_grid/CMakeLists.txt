# PicoTTL Example 08: 8x8 grid (framebuffer addressing validation).

add_executable(picottl_example_08_grid
    main.cpp
)

pico_set_program_name(picottl_example_08_grid "PicoTTL Example 08 - Grid")
pico_set_program_version(picottl_example_08_grid "0.1")

target_link_libraries(picottl_example_08_grid PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_08_grid)
