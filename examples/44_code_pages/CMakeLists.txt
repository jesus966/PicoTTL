# PicoTTL Example 44: IBM code pages (CP437 vs CP850).

add_executable(picottl_example_44_code_pages
    main.cpp
)

pico_set_program_name(picottl_example_44_code_pages "PicoTTL Example 44 - Code Pages")
pico_set_program_version(picottl_example_44_code_pages "0.1")

target_link_libraries(picottl_example_44_code_pages PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_44_code_pages)
