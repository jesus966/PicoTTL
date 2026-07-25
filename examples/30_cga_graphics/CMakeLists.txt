# PicoTTL Example 30: CGA color graphics (full Graphics pipeline in 16 colors).

add_executable(picottl_example_30_cga_graphics
    main.cpp
)

pico_set_program_name(picottl_example_30_cga_graphics "PicoTTL Example 30 - CGA Graphics")
pico_set_program_version(picottl_example_30_cga_graphics "0.1")

target_link_libraries(picottl_example_30_cga_graphics PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_30_cga_graphics)
