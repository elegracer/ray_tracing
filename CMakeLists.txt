cmake_minimum_required(VERSION 3.14)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules")
project(ray_tracing LANGUAGES C CXX VERSION 0.0.1.0)

include(CMakeDependentOption)

find_program(CCACHE ccache)
if(CCACHE)
    message(STATUS "ccache found")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
else()
    message(STATUS "ccache not found")
endif()

# Set the output directory for libraries and executables
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)

# set global cxx standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# always generate position independent code, no matter static or shared library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# disable cpp extensions
set(CMAKE_CXX_EXTENSIONS OFF)
# # link time optimization
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)


set(THIRDPARTY_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/3rdparty)

find_package(Eigen3 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(range-v3 CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

# options
option(OPTION_A "..." ON)

# core library
configure_file(cmake/version.h.in include/core/version.h)

add_library(core SHARED)
target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/include/core/version.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/core.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/core.h
)
target_include_directories(core
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(core
    PUBLIC
        Eigen3::Eigen
        fmt::fmt
        cxxopts::cxxopts
        range-v3::meta range-v3::concepts range-v3::range-v3
        ${OpenCV_LIBS}
)

# utils
add_executable(use_core)
target_sources(use_core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/use_core.cpp
)
target_link_libraries(use_core
    PRIVATE
        core
)
