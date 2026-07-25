# PicoTTL CGA CRT Diagnostics - the permanent CGA backend validation
# tool. Sibling of the MDA diagnostics; shares the pattern framework
# and generic catalog through picottl_diagnostics_common while the two
# applications evolve independently.

add_executable(picottl_cga_crt_diagnostics
    main.cpp
)

pico_set_program_name(picottl_cga_crt_diagnostics "PicoTTL CGA CRT Diagnostics")
pico_set_program_version(picottl_cga_crt_diagnostics "0.1")

target_link_libraries(picottl_cga_crt_diagnostics PRIVATE
    picottl_diagnostics_common
)

pico_add_extra_outputs(picottl_cga_crt_diagnostics)
