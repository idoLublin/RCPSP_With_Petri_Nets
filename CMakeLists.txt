cmake_minimum_required(VERSION 3.16)
project(rcpsp_lber CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# OR-Tools provides CP-SAT (EER/GER feasibility IPs) and Glop (LP). Only needed
# for the EER/GER variants (depth >= 5); depths 1-4 build fine without it.
find_package(ortools CONFIG REQUIRED)

add_executable(Driver HOG2/RCPSP/Driver.cpp)
target_compile_definitions(Driver PRIVATE USE_ORTOOLS)
target_link_libraries(Driver PRIVATE ortools::ortools)
