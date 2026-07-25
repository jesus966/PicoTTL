# PicoTTL MDA CRT Diagnostics - the permanent MDA-family backend
# validation tool. Shares the pattern framework and generic pattern
# catalog with its sibling applications through
# picottl_diagnostics_common; the applications themselves stay
# independent.

add_executable(picottl_mda_crt_diagnostics
    main.cpp
)

pico_set_program_name(picottl_mda_crt_diagnostics "PicoTTL MDA CRT Diagnostics")
pico_set_program_version(picottl_mda_crt_diagnostics "0.1")

target_link_libraries(picottl_mda_crt_diagnostics PRIVATE
    picottl_diagnostics_common
)

pico_add_extra_outputs(picottl_mda_crt_diagnostics)
