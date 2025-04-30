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

# if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT ANDROID)
#     add_compile_options(-stdlib=libc++)
#     add_link_options(-stdlib=libc++)
#     set(CMAKE_CXX_FLAGS "-stdlib=libc++")
# endif()

set(THIRDPARTY_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/3rdparty)

find_package(Eigen3 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(range-v3 CONFIG REQUIRED)
find_package(proxy CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_path(ICECREAM_CPP_INCLUDE_DIRS "icecream.hpp")
find_package(TBB CONFIG REQUIRED)
find_package(indicators CONFIG REQUIRED)

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
        ${ICECREAM_CPP_INCLUDE_DIRS}
)
target_link_libraries(core
    PUBLIC
        Eigen3::Eigen
        fmt::fmt
        argparse::argparse
        range-v3::meta range-v3::concepts range-v3::range-v3
        msft_proxy
        ${OpenCV_LIBS}
        TBB::tbb TBB::tbbmalloc
        indicators::indicators
)

# utils
add_executable(estimate_halfway)
target_sources(estimate_halfway PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/estimate_halfway.cpp)
target_link_libraries(estimate_halfway PRIVATE core)

add_executable(estimate_pi)
target_sources(estimate_pi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/estimate_pi.cpp)
target_link_libraries(estimate_pi PRIVATE core)

add_executable(integrate_x_sq)
target_sources(integrate_x_sq PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/integrate_x_sq.cpp)
target_link_libraries(integrate_x_sq PRIVATE core)

add_executable(render_scene)
target_sources(render_scene PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/render_scene.cpp)
target_link_libraries(render_scene PRIVATE core)

add_executable(sphere_importance)
target_sources(sphere_importance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/sphere_importance.cpp)
target_link_libraries(sphere_importance PRIVATE core)

add_executable(cos_cubed)
target_sources(cos_cubed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/cos_cubed.cpp)
target_link_libraries(cos_cubed PRIVATE core)
