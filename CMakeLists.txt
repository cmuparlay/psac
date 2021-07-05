# -------------------------------------------------------------------
#                       Build system for PSAC++
# -------------------------------------------------------------------
# Requirements:
#   - CMake version 3.13+
# -------------------------------------------------------------------

cmake_minimum_required(VERSION 3.13)
project(PSAC VERSION 1.0
             DESCRIPTION "Parallel Self-adjusting Computation for C++"
             LANGUAGES CXX)

# Set a default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
      "Choose the type of build, options are: Debug Release
RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

message(STATUS "--------------- General configuration -------------")
message(STATUS "CMake Generator:        ${CMAKE_GENERATOR}")
message(STATUS "Compiler:               ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type:             ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_FLAGS:        ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS  ${CMAKE_CXX_LINKER_FLAGS}")
message(STATUS "CMAKE_INSTALL_PREFIX:   ${CMAKE_INSTALL_PREFIX}" )

# Make sure -fno-omit-frame-pointer is set for profiling
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")

# -------------------------------------------------------------------
#                       Library definition
             
add_library(psac INTERFACE)
set(PSAC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
target_include_directories(psac INTERFACE ${PSAC_INCLUDE_DIR})
target_compile_features(psac INTERFACE cxx_std_17)

# Link against system threads (always)
find_package(Threads REQUIRED)
target_link_libraries(psac INTERFACE Threads::Threads)

# Enable 16 byte CAS
target_compile_definitions(psac INTERFACE -DMCX16)
target_compile_options(psac INTERFACE -mcx16)

# Link against tcmalloc, but only in optimized builds
# (because tcmalloc is not compatible with sanitizers)
find_library(MALLOC_LIB jemalloc)
if(NOT MALLOC_LIB)
  message(FATAL_ERROR "Could not find jemalloc.")
else()
  message("-- Found jemalloc: ${MALLOC_LIB}")
  target_link_libraries(psac INTERFACE optimized ${MALLOC_LIB})    
endif()

# -------------------------------------------------------------------
#                           Tests

message(STATUS "--------------------- Tests -----------------------")

# Set CMake options for GoogleTest
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)

# Download and configure GoogleTest
include(FetchContent)
FetchContent_Declare(googletest
  GIT_REPOSITORY  https://github.com/google/googletest.git
  GIT_TAG         master
)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  message(STATUS "testing: Configuring GoogleTest")
  FetchContent_Populate(googletest)  
  set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "")
  add_subdirectory(${googletest_SOURCE_DIR}
                   ${googletest_BINARY_DIR}
                   EXCLUDE_FROM_ALL)
endif()

# Include test targets
message(STATUS "testing: Enabled")
include(CTest)
add_subdirectory(tests)


# -------------------------------------------------------------------
#                           Benchmarks

message(STATUS "------------------- Benchmarks --------------------")

# Benchmark should not run its own unit tests
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

# Download Benchmark library
include(FetchContent)
FetchContent_Declare(benchmark
  GIT_REPOSITORY  https://github.com/google/benchmark
  GIT_TAG         master
)
FetchContent_GetProperties(benchmark)
if(NOT benchmark_POPULATED)
  message(STATUS "benchmarks: Configuring Google Benchmark")
  FetchContent_Populate(benchmark)
  set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "")
  add_subdirectory(${benchmark_SOURCE_DIR}
                   ${benchmark_BINARY_DIR}
                   EXCLUDE_FROM_ALL)
endif()

# Include benchmark targets
message(STATUS "benchmarks: Enabled")
add_subdirectory(benchmarks)

# -------------------------------------------------------------------
#                           Examples

add_subdirectory(examples)


# -------------------------------------------------------------------
#                           Reports

add_subdirectory(reports)


message(STATUS "---------------------------------------------------")

