# PicoTTL Demonstration 97: IBM Color Graphics Adapter.
# Official showcase demonstration (see docs/architecture.md).

add_executable(picottl_demo_97_cga
    main.cpp
)

pico_set_program_name(picottl_demo_97_cga "PicoTTL CGA Demonstration")
pico_set_program_version(picottl_demo_97_cga "1.0")

target_link_libraries(picottl_demo_97_cga PRIVATE
    picottl
    picottl_demo_common
    pico_stdlib
)

pico_add_extra_outputs(picottl_demo_97_cga)
