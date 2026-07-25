# PicoTTL Example 28: CGA 320x200 mode and 640<->320 switching.

add_executable(picottl_example_28_cga_320
    main.cpp
)

pico_set_program_name(picottl_example_28_cga_320 "PicoTTL Example 28 - CGA 320x200")
pico_set_program_version(picottl_example_28_cga_320 "0.1")

target_link_libraries(picottl_example_28_cga_320 PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_28_cga_320)
