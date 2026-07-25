# PicoTTL Example 35: EGA solid colors (64-color full-field cycle).

add_executable(picottl_example_35_ega_solid_colors
    main.cpp
)

pico_set_program_name(picottl_example_35_ega_solid_colors "PicoTTL Example 35 - EGA Solid Colors")
pico_set_program_version(picottl_example_35_ega_solid_colors "0.1")

target_link_libraries(picottl_example_35_ega_solid_colors PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_35_ega_solid_colors)
