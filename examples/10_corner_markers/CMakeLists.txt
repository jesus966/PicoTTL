# PicoTTL Example 10: corner markers (origin and clipping validation).

add_executable(picottl_example_10_corner_markers
    main.cpp
)

pico_set_program_name(picottl_example_10_corner_markers "PicoTTL Example 10 - Corner Markers")
pico_set_program_version(picottl_example_10_corner_markers "0.1")

target_link_libraries(picottl_example_10_corner_markers PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_10_corner_markers)
