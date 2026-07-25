# PicoTTL Example 18: text scrolling (clear/scroll and fast row copy).

add_executable(picottl_example_18_text_scrolling
    main.cpp
)

pico_set_program_name(picottl_example_18_text_scrolling "PicoTTL Example 18 - Text Scrolling")
pico_set_program_version(picottl_example_18_text_scrolling "0.1")

target_link_libraries(picottl_example_18_text_scrolling PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_18_text_scrolling)
