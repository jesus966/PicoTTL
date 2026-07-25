# PicoTTL Demonstration 96: IBM Monochrome Display Adapter.
# Official showcase demonstration (see docs/architecture.md).

add_executable(picottl_demo_96_mda
    main.cpp
)

pico_set_program_name(picottl_demo_96_mda "PicoTTL MDA Demonstration")
pico_set_program_version(picottl_demo_96_mda "1.0")

target_link_libraries(picottl_demo_96_mda PRIVATE
    picottl
    picottl_demo_common
    pico_stdlib
)

pico_add_extra_outputs(picottl_demo_96_mda)
