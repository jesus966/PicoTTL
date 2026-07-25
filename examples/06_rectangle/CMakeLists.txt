# PicoTTL Example 06: rectangle with diagonals.

add_executable(picottl_example_06_rectangle
    main.cpp
)

pico_set_program_name(picottl_example_06_rectangle "PicoTTL Example 06 - Rectangle")
pico_set_program_version(picottl_example_06_rectangle "0.1")

target_link_libraries(picottl_example_06_rectangle PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_06_rectangle)
