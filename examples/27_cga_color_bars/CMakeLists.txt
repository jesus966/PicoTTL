# PicoTTL Example 27: CGA color bars (RGBI staircase).

add_executable(picottl_example_27_cga_color_bars
    main.cpp
)

pico_set_program_name(picottl_example_27_cga_color_bars "PicoTTL Example 27 - CGA Color Bars")
pico_set_program_version(picottl_example_27_cga_color_bars "0.1")

target_link_libraries(picottl_example_27_cga_color_bars PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_27_cga_color_bars)
