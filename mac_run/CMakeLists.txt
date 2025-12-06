cmake_minimum_required(VERSION 3.20)
project(RCPSP_With_Petri_Nets LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RCPSP_ENABLE_HCACHE "Cache heuristic results across runs" ON)

find_package(Threads REQUIRED)

# On macOS with Homebrew, OpenMP (libomp) is keg-only and requires special handling
if(APPLE)
    # Try to find libomp in Homebrew's location
    execute_process(
        COMMAND brew --prefix libomp
        OUTPUT_VARIABLE LIBOMP_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(LIBOMP_PREFIX)
        set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include")
        set(OpenMP_C_LIB_NAMES "omp")
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include")
        set(OpenMP_CXX_LIB_NAMES "omp")
        set(OpenMP_omp_LIBRARY "${LIBOMP_PREFIX}/lib/libomp.dylib")
    endif()
endif()

find_package(OpenMP REQUIRED)

add_executable(rcpsp_driver
    ../HOG2/RCPSP/Driver.cpp
)

# Ensure the compiler can resolve the headers that live inside HOG2/RCPSP
# even when the sources use relative include paths.
target_include_directories(rcpsp_driver PRIVATE
    ..
    ../HOG2
    ../HOG2/RCPSP
)

# Add aggressive optimization flags for maximum performance
target_compile_options(rcpsp_driver PRIVATE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>
    $<$<CONFIG:Release>:-DNDEBUG>
    $<$<CONFIG:Release>:-flto>
)

target_link_libraries(rcpsp_driver PRIVATE
    Threads::Threads
    OpenMP::OpenMP_CXX
)

# Define OS_MAC on macOS for proper OpenGL header includes
if(APPLE)
    target_compile_definitions(rcpsp_driver PRIVATE OS_MAC)
endif()

if(RCPSP_ENABLE_HCACHE)
    target_compile_definitions(rcpsp_driver PRIVATE RCPSP_ENABLE_HCACHE=1)
endif()

# Make sure we only build optimized binaries when the user passes -DCMAKE_BUILD_TYPE
# (cmake defaults to no type on multi-config generators but Ninja/Make respect this variable).
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
