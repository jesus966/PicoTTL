# PicoTTL Example 11: concentric rectangles (geometry and clipping validation).

add_executable(picottl_example_11_concentric_rectangles
    main.cpp
)

pico_set_program_name(picottl_example_11_concentric_rectangles "PicoTTL Example 11 - Concentric Rectangles")
pico_set_program_version(picottl_example_11_concentric_rectangles "0.1")

target_link_libraries(picottl_example_11_concentric_rectangles PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_11_concentric_rectangles)
