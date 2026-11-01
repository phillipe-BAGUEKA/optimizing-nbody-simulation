cmake_minimum_required(VERSION 3.25)

project(
    optimizing_nbody_simulation
    VERSION 0.1.0
    DESCRIPTION "Direct 3D N-body simulation learning project"
    LANGUAGES CXX
)

find_package(OpenMP REQUIRED COMPONENTS CXX)

add_executable(openmp_smoke_check src/openmp_smoke_check.cpp)

target_compile_features(openmp_smoke_check PRIVATE cxx_std_20)
set_target_properties(
    openmp_smoke_check
    PROPERTIES
        CXX_EXTENSIONS OFF
)

target_link_libraries(openmp_smoke_check PRIVATE OpenMP::OpenMP_CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(
        openmp_smoke_check
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
    )
endif()

include(CTest)

if(BUILD_TESTING)
    add_test(NAME openmp_smoke_check COMMAND openmp_smoke_check)
endif()
