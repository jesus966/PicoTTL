# PicoTTL Example 14: animation test (continuous framebuffer updates).

add_executable(picottl_example_14_animation
    main.cpp
)

pico_set_program_name(picottl_example_14_animation "PicoTTL Example 14 - Animation")
pico_set_program_version(picottl_example_14_animation "0.1")

target_link_libraries(picottl_example_14_animation PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_14_animation)
