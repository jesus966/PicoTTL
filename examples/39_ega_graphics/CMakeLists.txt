# PicoTTL Example 39: EGA color graphics (full Graphics pipeline in 64 colors).

add_executable(picottl_example_39_ega_graphics
    main.cpp
)

pico_set_program_name(picottl_example_39_ega_graphics "PicoTTL Example 39 - EGA Graphics")
pico_set_program_version(picottl_example_39_ega_graphics "0.1")

target_link_libraries(picottl_example_39_ega_graphics PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_39_ega_graphics)
