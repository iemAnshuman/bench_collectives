cmake_minimum_required(VERSION 3.20)
project(HPXCollectives CXX)

# Find HPX
find_package(HPX REQUIRED)

# Create executable
add_executable(test_hpx hpx_benchmark.cpp)

# Link with HPX
target_link_libraries(test_hpx HPX::hpx)

# Add recommended compile flags
target_compile_features(test_hpx PRIVATE cxx_std_17)

