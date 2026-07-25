# PicoTTL Example 34: IBM EGA sync generation (black screen, 350-line raster).

add_executable(picottl_example_34_ega_sync
    main.cpp
)

pico_set_program_name(picottl_example_34_ega_sync "PicoTTL Example 34 - EGA Sync")
pico_set_program_version(picottl_example_34_ega_sync "0.1")

target_link_libraries(picottl_example_34_ega_sync PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_34_ega_sync)
