# PicoTTL Example 13: deterministic graphics stress test (visual regression).

add_executable(picottl_example_13_stress_test
    main.cpp
)

pico_set_program_name(picottl_example_13_stress_test "PicoTTL Example 13 - Stress Test")
pico_set_program_version(picottl_example_13_stress_test "0.1")

target_link_libraries(picottl_example_13_stress_test PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(picottl_example_13_stress_test)
