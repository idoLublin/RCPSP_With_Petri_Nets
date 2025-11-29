cmake_minimum_required(VERSION 3.20)
project(RCPSP_With_Petri_Nets LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RCPSP_ENABLE_HCACHE "Cache heuristic results across runs" ON)

find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(rcpsp_driver
    HOG2/RCPSP/Driver.cpp
)

# Ensure the compiler can resolve the headers that live inside HOG2/RCPSP
# even when the sources use relative include paths.
target_include_directories(rcpsp_driver PRIVATE
    HOG2
    HOG2/RCPSP
)

target_link_libraries(rcpsp_driver PRIVATE
    Threads::Threads
    OpenMP::OpenMP_CXX
)

if(RCPSP_ENABLE_HCACHE)
    target_compile_definitions(rcpsp_driver PRIVATE RCPSP_ENABLE_HCACHE=1)
endif()

# Make sure we only build optimized binaries when the user passes -DCMAKE_BUILD_TYPE
# (cmake defaults to no type on multi-config generators but Ninja/Make respect this variable).
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
