# PicoTTL Example 19: intensity bars (2 bpp electrical validation).

add_executable(picottl_example_19_intensity_bars
    main.cpp
)

pico_set_program_name(picottl_example_19_intensity_bars "PicoTTL Example 19 - Intensity Bars")
pico_set_program_version(picottl_example_19_intensity_bars "0.1")

target_link_libraries(picottl_example_19_intensity_bars PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_19_intensity_bars)
