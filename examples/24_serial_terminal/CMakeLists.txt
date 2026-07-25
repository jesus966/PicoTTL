# PicoTTL Example 24: ANSI serial terminal (USB CDC -> AnsiTerminal -> Terminal).

add_executable(picottl_example_24_serial_terminal
    main.cpp
)

pico_set_program_name(picottl_example_24_serial_terminal "PicoTTL Example 24 - ANSI Serial Terminal")
pico_set_program_version(picottl_example_24_serial_terminal "0.1")

# The example owns the USB CDC interface.
pico_enable_stdio_usb(picottl_example_24_serial_terminal 1)
pico_enable_stdio_uart(picottl_example_24_serial_terminal 0)

target_link_libraries(picottl_example_24_serial_terminal PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_24_serial_terminal)
