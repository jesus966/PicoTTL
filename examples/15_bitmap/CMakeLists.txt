# PicoTTL Example 15: bitmap rendering (drawBitmap validation).

add_executable(picottl_example_15_bitmap
    main.cpp
)

pico_set_program_name(picottl_example_15_bitmap "PicoTTL Example 15 - Bitmap")
pico_set_program_version(picottl_example_15_bitmap "0.1")

target_link_libraries(picottl_example_15_bitmap PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_15_bitmap)
