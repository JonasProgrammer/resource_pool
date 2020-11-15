cmake_minimum_required(VERSION 3.12)
project(PackageTest CXX)

find_package(resource_pool REQUIRED)

add_executable(example ../examples/async/strand.cc)
target_link_libraries(example PRIVATE elsid::resource_pool)
