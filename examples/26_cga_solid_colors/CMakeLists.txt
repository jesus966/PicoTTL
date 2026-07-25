# PicoTTL Example 26: CGA solid colors (16-color full-field cycle).

add_executable(picottl_example_26_cga_solid_colors
    main.cpp
)

pico_set_program_name(picottl_example_26_cga_solid_colors "PicoTTL Example 26 - CGA Solid Colors")
pico_set_program_version(picottl_example_26_cga_solid_colors "0.1")

target_link_libraries(picottl_example_26_cga_solid_colors PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_26_cga_solid_colors)
