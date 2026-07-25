# PicoTTL Example 17: Hello World (complete text pipeline validation).

add_executable(picottl_example_17_hello_world
    main.cpp
)

pico_set_program_name(picottl_example_17_hello_world "PicoTTL Example 17 - Hello World")
pico_set_program_version(picottl_example_17_hello_world "0.1")

target_link_libraries(picottl_example_17_hello_world PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_17_hello_world)
