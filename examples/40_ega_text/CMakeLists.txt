# PicoTTL Example 40: EGA text (IBM 8x14 font, default palette, 80x25).

add_executable(picottl_example_40_ega_text
    main.cpp
)

pico_set_program_name(picottl_example_40_ega_text "PicoTTL Example 40 - EGA Text")
pico_set_program_version(picottl_example_40_ega_text "0.1")

target_link_libraries(picottl_example_40_ega_text PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_40_ega_text)
