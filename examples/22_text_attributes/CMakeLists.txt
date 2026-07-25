# PicoTTL Example 22: text attributes (bright, underline, inverse video).

add_executable(picottl_example_22_text_attributes
    main.cpp
)

pico_set_program_name(picottl_example_22_text_attributes "PicoTTL Example 22 - Text Attributes")
pico_set_program_version(picottl_example_22_text_attributes "0.1")

target_link_libraries(picottl_example_22_text_attributes PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_22_text_attributes)
