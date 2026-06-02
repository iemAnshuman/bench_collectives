cmake_minimum_required(VERSION 3.16)
project(benchmark_collectives_mpi CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(MPI REQUIRED)

# cxxopts: use an installed copy if available, otherwise fetch it.
find_package(cxxopts QUIET)
if(NOT cxxopts_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG        v3.2.0
  )
  FetchContent_MakeAvailable(cxxopts)
endif()

add_executable(benchmark_collectives_mpi mpi_benchmark.cpp)
target_link_libraries(benchmark_collectives_mpi PRIVATE MPI::MPI_CXX cxxopts::cxxopts)
target_compile_options(benchmark_collectives_mpi PRIVATE -Wall -Wextra)

# Keep the binary at build/mpi/bin/ so the documented run paths stay valid.
set_target_properties(benchmark_collectives_mpi PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
