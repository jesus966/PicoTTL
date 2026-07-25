# PicoTTL Example 43: host image display (PC -> USB CDC -> monitor).

add_executable(picottl_example_43_host_image
    main.cpp
)

pico_set_program_name(picottl_example_43_host_image "PicoTTL Example 43 - Host Image")
pico_set_program_version(picottl_example_43_host_image "0.1")

target_link_libraries(picottl_example_43_host_image PRIVATE
    picottl
    pico_stdlib
)

pico_enable_stdio_usb(picottl_example_43_host_image 1)
pico_enable_stdio_uart(picottl_example_43_host_image 0)

pico_add_extra_outputs(picottl_example_43_host_image)
