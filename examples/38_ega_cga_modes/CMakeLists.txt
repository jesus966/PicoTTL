# PicoTTL Example 38: EGA running CGA-compatible 200-line modes (cross-crystal reclocking).

add_executable(picottl_example_38_ega_cga_modes
    main.cpp
)

pico_set_program_name(picottl_example_38_ega_cga_modes "PicoTTL Example 38 - EGA CGA Modes")
pico_set_program_version(picottl_example_38_ega_cga_modes "0.1")

target_link_libraries(picottl_example_38_ega_cga_modes PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_38_ega_cga_modes)
