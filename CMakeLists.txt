# --------------------------
# Setup
# --------------------------
cmake_minimum_required(VERSION 3.16)
project(Collatz LANGUAGES CXX)

# Cuda is optional.
find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND)
    message(STATUS "CUDA found, enabling CUDA builds")
    add_definitions(-DHAVE_CUDA)
    enable_language(CUDA)
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
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g -march=native" CACHE STRING "Flags used by the compiler during Release builds." FORCE)


# Default build type = Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type (Release or Debug)" FORCE)
endif()

# Extra compile flags
add_compile_options(-Wall -Wextra)

# Include directories.  Absl and CRoaring export their own info.
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
    roaring
)



# --------------------------
# Abseil's CMake build system
# --------------------------
add_subdirectory(${CMAKE_SOURCE_DIR}/src/include/abseil-cpp)



# --------------------------
# CRoaring's CMake build system
# --------------------------
set(ENABLE_ROARING_TESTS OFF CACHE BOOL "" FORCE)
set(ROARING_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/src/include/CRoaring)



# --------------------------
# Shared objects
# --------------------------
add_library(logging OBJECT src/logging.cpp)



# --------------------------
# Programs
# --------------------------
set(CPU_PROGRAMS
    collatz_compression
    coverage
    junk
    performance_stats
    sieve_stuff
    single_collatz
)

set(GPU_PROGRAMS
    peak_by_bit
    step_counter
)

# CPU-only programs
foreach(prog ${CPU_PROGRAMS})
    add_executable(${prog} src/${prog}.cpp $<TARGET_OBJECTS:logging>)
    target_link_libraries(${prog} PRIVATE ${PROJECT_LIBS})
endforeach()

# GPU programs
if(CUDAToolkit_FOUND)
    foreach(prog ${GPU_PROGRAMS})
        set(GPU_SRC src/collatz/${prog}.cu)
        if(EXISTS ${CMAKE_SOURCE_DIR}/${GPU_SRC})
            add_executable(${prog} src/${prog}.cpp ${GPU_SRC} $<TARGET_OBJECTS:logging>)
            set_target_properties(${prog} PROPERTIES
                CUDA_SEPARABLE_COMPILATION ON
                CUDA_STANDARD 20
                CUDA_STANDARD_REQUIRED ON
                CUDA_ARCHITECTURES "89;90"
            )
            target_link_libraries(${prog} PRIVATE ${PROJECT_LIBS})
        else()
            message(WARNING "GPU source file ${GPU_SRC} not found for program ${prog}")
        endif()
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
    target_link_libraries(test__${test} PRIVATE ${PROJECT_LIBS})
    add_test(NAME ${test} COMMAND test__${test})
endforeach()
