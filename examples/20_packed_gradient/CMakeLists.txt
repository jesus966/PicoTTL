# PicoTTL Example 20: packed gradient (2 bpp packing and bit-order validation).

add_executable(picottl_example_20_packed_gradient
    main.cpp
)

pico_set_program_name(picottl_example_20_packed_gradient "PicoTTL Example 20 - Packed Gradient")
pico_set_program_version(picottl_example_20_packed_gradient "0.1")

target_link_libraries(picottl_example_20_packed_gradient PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_20_packed_gradient)
