# PicoTTL Example 12: radiating lines (Bresenham slope coverage).

add_executable(picottl_example_12_radiating_lines
    main.cpp
)

pico_set_program_name(picottl_example_12_radiating_lines "PicoTTL Example 12 - Radiating Lines")
pico_set_program_version(picottl_example_12_radiating_lines "0.1")

target_link_libraries(picottl_example_12_radiating_lines PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_12_radiating_lines)
