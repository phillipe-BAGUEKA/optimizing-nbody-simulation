cmake_minimum_required(VERSION 3.25)

project(
    optimizing_nbody_simulation
    VERSION 0.1.0
    DESCRIPTION "Direct 3D N-body simulation learning project"
    LANGUAGES CXX
)

find_package(OpenMP REQUIRED COMPONENTS CXX)

# Apply the same language mode and warnings to each compiled target.
function(configure_nbody_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endfunction()

add_executable(openmp_smoke_check src/openmp_smoke_check.cpp)
configure_nbody_target(openmp_smoke_check)
target_link_libraries(openmp_smoke_check PRIVATE OpenMP::OpenMP_CXX)

add_library(nbody STATIC src/gravity.cpp src/simulation.cpp)
configure_nbody_target(nbody)
target_include_directories(nbody PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(nbody PUBLIC OpenMP::OpenMP_CXX)

add_executable(nbody_demo src/main.cpp)
configure_nbody_target(nbody_demo)
target_link_libraries(nbody_demo PRIVATE nbody)

add_executable(nbody_benchmark benchmarks/benchmark.cpp)
configure_nbody_target(nbody_benchmark)
target_link_libraries(nbody_benchmark PRIVATE nbody)

include(CTest)

if(BUILD_TESTING)
    add_test(NAME openmp_smoke_check COMMAND openmp_smoke_check)

    add_executable(vec3_tests tests/vec3_tests.cpp)
    configure_nbody_target(vec3_tests)
    target_include_directories(vec3_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
    add_test(NAME vec3_tests COMMAND vec3_tests)

    add_executable(simulation_tests tests/simulation_tests.cpp)
    configure_nbody_target(simulation_tests)
    target_link_libraries(simulation_tests PRIVATE nbody)
    add_test(NAME simulation_tests COMMAND simulation_tests)
    add_test(NAME benchmark_equivalence COMMAND nbody_benchmark campaign 17 3 42 2 - 1 2)
    add_test(NAME benchmark_zero_steps COMMAND nbody_benchmark campaign 5 0 42 1 - 2)
    add_test(NAME benchmark_arrivals COMMAND nbody_benchmark arrivals 7 2 42 2 2 4)
    add_test(NAME schedule_comparison COMMAND nbody_benchmark compare 131 3 42 2 - 1 2 4)
    add_test(NAME schedule_arrivals COMMAND nbody_benchmark schedule-arrivals 131 3 42 2 2 4)
    add_test(NAME demo_dynamic COMMAND nbody_demo --solver dynamic16 --threads 4 --diagnostics)

    add_test(
        NAME demo_sequential
        COMMAND nbody_demo --solver sequential --case two-body --steps 1 --dt 0.5
    )
    add_test(
        NAME demo_openmp
        COMMAND nbody_demo --solver openmp --case multi-body --steps 10 --dt 0.01 --threads 4
    )
    add_test(
        NAME cli_validation
        COMMAND "${CMAKE_COMMAND}" "-DDEMO=$<TARGET_FILE:nbody_demo>"
                -P "${PROJECT_SOURCE_DIR}/tests/cli_validation.cmake"
    )
endif()
