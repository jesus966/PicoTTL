# PicoTTL Example 21: pixel encoding diagnostic (hardware diagnostic tool).

add_executable(picottl_example_21_pixel_diagnostic
    main.cpp
)

pico_set_program_name(picottl_example_21_pixel_diagnostic "PicoTTL Example 21 - Pixel Diagnostic")
pico_set_program_version(picottl_example_21_pixel_diagnostic "0.1")

target_link_libraries(picottl_example_21_pixel_diagnostic PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_21_pixel_diagnostic)
