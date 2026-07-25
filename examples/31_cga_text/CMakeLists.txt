# PicoTTL Example 31: CGA text (IBM 8x8 font, 16 colors, 80x25).

add_executable(picottl_example_31_cga_text
    main.cpp
)

pico_set_program_name(picottl_example_31_cga_text "PicoTTL Example 31 - CGA Text")
pico_set_program_version(picottl_example_31_cga_text "0.1")

target_link_libraries(picottl_example_31_cga_text PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_31_cga_text)
