# --------------------------
# Setup
# --------------------------
cmake_minimum_required(VERSION 3.16)
project(Collatz LANGUAGES CXX)

# Cuda is optional.
find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND)
    if(CUDAToolkit_VERSION VERSION_LESS "11.6")
        message(FATAL_ERROR
            "CUDA Toolkit >= 11.6 is required for __int128 device support. "
            "Found: ${CUDAToolkit_VERSION}"
        )
    endif()
    message(STATUS "CUDA found, enabling CUDA builds")
    add_definitions(-DHAVE_CUDA)
    enable_language(CUDA)
endif()

# Write binaries to a bin dir.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Setup and expose stride settings.
set(COLLATZ_NODE_INIT_STRIDE_SIZE_FW     9 CACHE STRING "Stride size used while building a Node for Fixed-Width types")
set(COLLATZ_NODE_INIT_STRIDE_SIZE_GMP   16 CACHE STRING "Stride size used while building a Node for GMP (mpz_class) type")
set(COLLATZ_VERIFY_STRIDE_SIZE_FW       12 CACHE STRING "Stride size used while verifying fixed-width values")
set(COLLATZ_VERIFY_STRIDE_SIZE_GMP      16 CACHE STRING "Stride size used while verifying GMP values")
set(COLLATZ_VERIFY_STRIDE_SIZE_FW_CUDA  13 CACHE STRING "Stride size used while verifying fixed-width values on cuda")
add_compile_definitions(
    COLLATZ_NODE_INIT_STRIDE_SIZE_FW=${COLLATZ_NODE_INIT_STRIDE_SIZE_FW}
    COLLATZ_NODE_INIT_STRIDE_SIZE_GMP=${COLLATZ_NODE_INIT_STRIDE_SIZE_GMP}
    COLLATZ_VERIFY_STRIDE_SIZE_FW=${COLLATZ_VERIFY_STRIDE_SIZE_FW}
    COLLATZ_VERIFY_STRIDE_SIZE_GMP=${COLLATZ_VERIFY_STRIDE_SIZE_GMP}
    COLLATZ_VERIFY_STRIDE_SIZE_FW_CUDA=${COLLATZ_VERIFY_STRIDE_SIZE_FW_CUDA}
)


# --------------------------
# FetchContent
# --------------------------
# Use FetchContent for external libs.
include(FetchContent)

# Configure Dependencies
# -- roaring
set(ENABLE_ROARING_TESTS OFF CACHE BOOL "" FORCE)
set(ROARING_BUILD_STATIC ON CACHE BOOL "" FORCE)
# -- zstd
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Helper to keep things consistent.
function(add_git_dependency name repo tag)
    message(STATUS "Fetching ${name} (${tag})...")
    FetchContent_Declare(
        ${name}
        GIT_REPOSITORY ${repo}
        GIT_TAG ${tag}
        GIT_SHALLOW TRUE
        # SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/${name}"
    )
    FetchContent_MakeAvailable(${name})
endfunction()

# Abseil
add_git_dependency(
    abseil-cpp
    https://github.com/abseil/abseil-cpp.git
    20250814.0
)

# CLI11
add_git_dependency(
    CLI11
    https://github.com/CLIUtils/CLI11.git
    v2.5.0
)

# CRoaring
add_git_dependency(
    CRoaring
    https://github.com/RoaringBitmap/CRoaring.git
    v4.6.1
)

# Spdlog
add_git_dependency(
    spdlog
    https://github.com/gabime/spdlog.git
    v1.15.3
)

# Zstd
add_git_dependency(
    zstd
    https://github.com/facebook/zstd.git
    v1.5.7
)
# (It doesn't emit directory info reliably.  Add it manually.  Then link against libzstd_static lib.)
add_subdirectory(
    ${CMAKE_BINARY_DIR}/_deps/zstd-src/build/cmake
    ${CMAKE_BINARY_DIR}/_deps/zstd-build
)


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
set(CMAKE_CUDA_FLAGS_RELEASE "-O2")


# Default build type = Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type (Release or Debug)" FORCE)
endif()

# Extra compile flags
add_compile_options(-Wall -Wextra)

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
    libzstd_static
    absl::flat_hash_map
    absl::hash
    roaring
    CLI11::CLI11
)



# --------------------------
# Shared objects
# --------------------------
add_library(logging STATIC src/logging.cpp)
target_link_libraries(logging PUBLIC absl::hash spdlog::spdlog)

# Create a single GPU object with our kernels.
add_library(collatz_gpu OBJECT
    src/collatz/peak_by_bit.cu
    src/collatz/step_counter.cu
    src/collatz/verifier_gpu_kernel.cu
)
# Set the properties on the GPU object.
set_target_properties(collatz_gpu PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_STANDARD 20
    CUDA_STANDARD_REQUIRED ON
    CUDA_ARCHITECTURES "89;90"
)

# --------------------------
# Programs
# --------------------------
set(CPU_PROGRAMS
    collatz_compression
    coverage
    junk
    performance_stats
    single_collatz
)

set(GPU_PROGRAMS
    peak_by_bit
    step_counter
    verifier
)

# CPU-only programs.  Just a simple loop.
foreach(prog ${CPU_PROGRAMS})
    add_executable(${prog} src/${prog}.cpp)
    target_link_libraries(${prog} PRIVATE logging)
    target_link_libraries(${prog} PRIVATE ${PROJECT_LIBS})
endforeach()

# GPU programs.  Also a simple loop now that we're using an object.
if(CUDAToolkit_FOUND)
    foreach(prog ${GPU_PROGRAMS})
        add_executable(${prog} src/${prog}.cpp $<TARGET_OBJECTS:collatz_gpu>)
        set_target_properties(${prog} PROPERTIES
            CUDA_SEPARABLE_COMPILATION ON
            CUDA_STANDARD 20
            CUDA_STANDARD_REQUIRED ON
            CUDA_ARCHITECTURES "89;90"
        )
        target_link_libraries(${prog} PRIVATE logging)
        target_link_libraries(${prog} PRIVATE ${PROJECT_LIBS})
    endforeach()
endif()



# --------------------------
# Tests
# --------------------------
set(TESTS
    abi
    binary_tree_coverage_constants
    binary_tree_coverage
    binary_tree_math
    binary_tree_options
    binary_tree
    bit
    collatz_affine_map
    collatz_affine_map_shortcut
    collatz_affine_stride
    collatz_constants
    collatz
    equality_helper
    exponents
    for_each
    gmp
    int128
    node_bitmap_traits
    node_bitmap
    node
    stream_helper
    string
    typedefs
    udl
    verifier_cpu
    verifier_gpu_result_data
    verifier_metric
    verifier_state
    verifier_tls_metric
    zstandard
)

enable_testing()
foreach(test ${TESTS})
    add_executable(test__${test} src/test/${test}.cpp)
    target_link_libraries(test__${test} PRIVATE logging)
    target_link_libraries(test__${test} PRIVATE ${PROJECT_LIBS})
    add_test(NAME ${test} COMMAND test__${test})
endforeach()
