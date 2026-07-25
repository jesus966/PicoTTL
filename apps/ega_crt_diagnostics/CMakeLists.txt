# PicoTTL EGA CRT Diagnostics - the permanent EGA backend validation
# tool. Sibling of the MDA and CGA diagnostics; shares the pattern
# framework and generic catalog through picottl_diagnostics_common
# while the applications evolve independently.

add_executable(picottl_ega_crt_diagnostics
    main.cpp
)

pico_set_program_name(picottl_ega_crt_diagnostics "PicoTTL EGA CRT Diagnostics")
pico_set_program_version(picottl_ega_crt_diagnostics "0.1")

target_link_libraries(picottl_ega_crt_diagnostics PRIVATE
    picottl_diagnostics_common
)

pico_add_extra_outputs(picottl_ega_crt_diagnostics)
