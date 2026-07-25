# PicoTTL Example 36: EGA color grid (all 64 colors at once).

add_executable(picottl_example_36_ega_color_grid
    main.cpp
)

pico_set_program_name(picottl_example_36_ega_color_grid "PicoTTL Example 36 - EGA Color Grid")
pico_set_program_version(picottl_example_36_ega_color_grid "0.1")

target_link_libraries(picottl_example_36_ega_color_grid PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_36_ega_color_grid)
