# PicoTTL Example 01: IBM MDA sync generation (black screen).

add_executable(picottl_example_01_mda_sync
    main.cpp
)

pico_set_program_name(picottl_example_01_mda_sync "PicoTTL Example 01 - MDA Sync")
pico_set_program_version(picottl_example_01_mda_sync "0.1")

target_link_libraries(picottl_example_01_mda_sync PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_01_mda_sync)
