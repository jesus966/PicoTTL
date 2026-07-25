# PicoTTL Example 16: single character (glyph rendering validation).

add_executable(picottl_example_16_single_character
    main.cpp
)

pico_set_program_name(picottl_example_16_single_character "PicoTTL Example 16 - Single Character")
pico_set_program_version(picottl_example_16_single_character "0.1")

target_link_libraries(picottl_example_16_single_character PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_16_single_character)
