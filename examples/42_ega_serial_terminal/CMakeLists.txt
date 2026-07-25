# PicoTTL Example 42: ANSI serial terminal on EGA (USB CDC, default palette).

add_executable(picottl_example_42_ega_serial_terminal
    main.cpp
)

pico_set_program_name(picottl_example_42_ega_serial_terminal "PicoTTL Example 42 - EGA ANSI Serial Terminal")
pico_set_program_version(picottl_example_42_ega_serial_terminal "0.1")

target_link_libraries(picottl_example_42_ega_serial_terminal PRIVATE
    picottl
    pico_stdlib
)

pico_enable_stdio_usb(picottl_example_42_ega_serial_terminal 1)
pico_enable_stdio_uart(picottl_example_42_ega_serial_terminal 0)

pico_add_extra_outputs(picottl_example_42_ega_serial_terminal)
