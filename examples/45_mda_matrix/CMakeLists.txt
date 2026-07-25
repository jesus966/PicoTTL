# PicoTTL Example 45: phosphor-persistence Matrix rain for MDA monitors.

add_executable(picottl_example_45_mda_matrix
    main.cpp
)

pico_set_program_name(picottl_example_45_mda_matrix "PicoTTL Example 45 - MDA Matrix")
pico_set_program_version(picottl_example_45_mda_matrix "1.0")

target_link_libraries(picottl_example_45_mda_matrix PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_45_mda_matrix)