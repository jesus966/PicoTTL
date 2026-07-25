# PicoTTL Demonstration 98: IBM Enhanced Graphics Adapter.
# Official showcase demonstration (see docs/architecture.md).

add_executable(picottl_demo_98_ega
    main.cpp
)

pico_set_program_name(picottl_demo_98_ega "PicoTTL EGA Demonstration")
pico_set_program_version(picottl_demo_98_ega "1.0")

target_link_libraries(picottl_demo_98_ega PRIVATE
    picottl
    picottl_demo_common
    pico_stdlib
)

pico_add_extra_outputs(picottl_demo_98_ega)
