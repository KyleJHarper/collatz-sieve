# --------------------------
# Setup
# --------------------------
cmake_minimum_required(VERSION 3.16)
project(Collatz LANGUAGES CXX)

# Cuda is optional.
find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND)
    enable_language(CUDA)

    add_library(collatz_gpu STATIC src/collatz/collatz_gpu.cu)
    set_target_properties(collatz_gpu PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_STANDARD 20
        CUDA_STANDARD_REQUIRED ON
        CUDA_ARCHITECTURES "89;90"
    )
    target_link_libraries(collatz_gpu PRIVATE ${PROJECT_LIBS})
endif()

# Write binaries to a bin dir.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})



# --------------------------
# Global settings
# --------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fno-omit-frame-pointer -fno-inline -fno-elide-constructors" CACHE STRING "Flags used by the compiler during Debug builds." FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -march=native" CACHE STRING "Flags used by the compiler during Release builds." FORCE)


# Default build type = Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type (Release or Debug)" FORCE)
endif()

# Extra compile flags
add_compile_options(-Wall -Wextra -pthread)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src/include/spdlog/include
    ${CMAKE_SOURCE_DIR}/src/include/CLI11/include/CLI
)

# Packages
find_package(TBB REQUIRED)
find_package(OpenMP REQUIRED)

# Libraries
find_library(JEMALLOC_LIB jemalloc REQUIRED)
find_library(GMP_LIB gmp REQUIRED)
find_library(GMPXX_LIB gmpxx REQUIRED)
set(PROJECT_LIBS
    ${JEMALLOC_LIB}
    ${GMP_LIB}
    ${GMPXX_LIB}
    TBB::tbb
    OpenMP::OpenMP_CXX
    absl::flat_hash_map
)



# --------------------------
# Abseil's CMake build system
# --------------------------
add_subdirectory(${CMAKE_SOURCE_DIR}/src/include/abseil-cpp)



# --------------------------
# Shared objects
# --------------------------
add_library(logging OBJECT src/logging.cpp)
# add_library(collatz_gpu OBJECT src/collatz_gpu.cu)
# set_target_properties(collatz_gpu PROPERTIES CUDA_SEPARABLE_COMPILATION ON)



# --------------------------
# Programs
# --------------------------
set(CPU_PROGRAMS
    ancestor_coverage
    coverage
    high_water_mark
    peak_by_bit
    performance_stats
    sieve_stuff
    single_collatz
    step_counter
)

set(GPU_PROGRAMS
    junk
    peak_by_bit_gpu
)

# CPU-only programs
foreach(prog ${CPU_PROGRAMS})
    add_executable(${prog} src/${prog}.cpp $<TARGET_OBJECTS:logging>)
    target_link_libraries(${prog} PRIVATE ${PROJECT_LIBS})
endforeach()

# GPU programs
if(TARGET collatz_gpu)
    foreach(prog ${GPU_PROGRAMS})
        add_executable(${prog} src/${prog}.cpp $<TARGET_OBJECTS:logging>)
        target_link_libraries(${prog} PRIVATE collatz_gpu ${PROJECT_LIBS})
        set_target_properties(${prog} PROPERTIES CUDA_ARCHITECTURES "89;90")
    endforeach()
endif()



# --------------------------
# Tests
# --------------------------
set(TESTS
    collatz_class
    node_class
    binary_tree_class
    forward_looking_cache_class
    sieve_class
)

enable_testing()
foreach(test ${TESTS})
    add_executable(test__${test} src/test/${test}.cpp $<TARGET_OBJECTS:logging>)
    if(TARGET collatz_gpu)
        target_link_libraries(test__${test} PRIVATE collatz_gpu ${PROJECT_LIBS})
        set_target_properties(test__${test} PROPERTIES CUDA_ARCHITECTURES "89;90")
    endif()
    target_link_libraries(test__${test} PRIVATE ${PROJECT_LIBS})
    add_test(NAME ${test} COMMAND test__${test})
endforeach()
