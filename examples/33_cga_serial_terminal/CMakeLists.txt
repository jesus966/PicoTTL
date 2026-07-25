# PicoTTL Example 33: ANSI serial terminal on CGA (USB CDC, 16 colors).

add_executable(picottl_example_33_cga_serial_terminal
    main.cpp
)

pico_set_program_name(picottl_example_33_cga_serial_terminal "PicoTTL Example 33 - CGA ANSI Serial Terminal")
pico_set_program_version(picottl_example_33_cga_serial_terminal "0.1")

target_link_libraries(picottl_example_33_cga_serial_terminal PRIVATE
    picottl
    pico_stdlib
)

pico_enable_stdio_usb(picottl_example_33_cga_serial_terminal 1)
pico_enable_stdio_uart(picottl_example_33_cga_serial_terminal 0)

pico_add_extra_outputs(picottl_example_33_cga_serial_terminal)
