# PicoTTL Example 25: IBM CGA sync generation (black screen).

add_executable(picottl_example_25_cga_sync
    main.cpp
)

pico_set_program_name(picottl_example_25_cga_sync "PicoTTL Example 25 - CGA Sync")
pico_set_program_version(picottl_example_25_cga_sync "0.1")

target_link_libraries(picottl_example_25_cga_sync PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_25_cga_sync)
