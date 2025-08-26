cmake_minimum_required(VERSION 3.16)
project(CollatzProject LANGUAGES CXX)

# --------------------------
# Global settings
# --------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type = Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the build type (Release or Debug)" FORCE)
endif()

# Extra compile flags
add_compile_options(-Wall -Wextra -pthread -fopenmp)

# Include directories (mimics -I in your Makefile)
include_directories(
    /usr/local/include
    ${CMAKE_SOURCE_DIR}/src/include/spdlog/include
    ${CMAKE_SOURCE_DIR}/src/include/CLI11/include/CLI
)

# Link directories (mimics -L in your Makefile)
link_directories(/usr/local/lib)

# Libraries (mimics LDFLAGS)
set(PROJECT_LIBS
    jemalloc
    tbb
    gmp
    gmpxx
)

# --------------------------
# Shared objects
# --------------------------
add_library(logging OBJECT src/logging.cpp)

# --------------------------
# Programs
# --------------------------
set(PROGRAMS
    ancestor_coverage
    coverage
    high_water_mark
    peak_by_bit
    performance_stats
    sieve_stuff
    single_collatz
)

foreach(prog ${PROGRAMS})
    add_executable(${prog} src/${prog}.cpp $<TARGET_OBJECTS:logging>)
    target_link_libraries(${prog} PRIVATE ${PROJECT_LIBS})
endforeach()

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

foreach(test ${TESTS})
    add_executable(test__${test} src/test/${test}.cpp $<TARGET_OBJECTS:logging>)
    target_link_libraries(test__${test} PRIVATE ${PROJECT_LIBS})
    add_test(NAME ${test} COMMAND test__${test})
endforeach()

