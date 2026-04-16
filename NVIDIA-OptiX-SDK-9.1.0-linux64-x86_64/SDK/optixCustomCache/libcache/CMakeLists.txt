#
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

cmake_minimum_required(VERSION 3.15)
project(libcache VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build ZeroMQ from local source (distributed with SDK)
set(LIBZMQ_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../support/zeromq-4.3.5")

# Configure ZeroMQ options before adding subdirectory
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "Build ZMQ tests")
set(BUILD_TESTS OFF CACHE BOOL "Build ZMQ tests") 
set(BUILD_SHARED OFF CACHE BOOL "Build ZMQ shared library")
set(BUILD_STATIC ON CACHE BOOL "Build ZMQ static library")
set(WITH_DOCS OFF CACHE BOOL "Build ZMQ documentation")
set(ENABLE_CPACK OFF CACHE BOOL "Disable ZMQ CPack packaging")
set(ZMQ_SKIP_INSTALL ON CACHE BOOL "Skip libzmq install() when built inside OptiX SDK")


# Suppress noisy STATUS messages from libzmq configuration (e.g. AsciiDoc/PkgConfig)
set(_saved_log_level ${CMAKE_MESSAGE_LOG_LEVEL})
set(CMAKE_MESSAGE_LOG_LEVEL WARNING)

# Add ZeroMQ as a subdirectory (uses libzmq's own targets)
add_subdirectory(${LIBZMQ_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/libzmq EXCLUDE_FROM_ALL)

# Restore message log level
set(CMAKE_MESSAGE_LOG_LEVEL ${_saved_log_level})

# Use libzmq-static target directly
set(ZMQ_LIBRARIES libzmq-static)

# Create the shared library (DLL on Windows, SO on Linux)
if(WIN32)
    add_library(libcache SHARED
        libcache.cpp
        libcache.h
        libcache.def
    )
else()
    add_library(libcache SHARED
        libcache.cpp
        libcache.h
    )
endif()

# Set target properties for shared library
set_target_properties(libcache PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF  # We'll use explicit exports
    OUTPUT_NAME "libcache"
)

# Link module definition file on Windows
if(WIN32)
    set_target_properties(libcache PROPERTIES
        LINK_FLAGS "/DEF:\"${CMAKE_CURRENT_SOURCE_DIR}/libcache.def\""
    )
endif()

# Include directories for headers
target_include_directories(libcache PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link ZeroMQ
target_link_libraries(libcache PRIVATE ${ZMQ_LIBRARIES})

# Add compile definitions for building the DLL
target_compile_definitions(libcache PRIVATE 
    BUILDING_LIBCACHE
    ZMQ_STATIC  # Required when linking against static ZeroMQ
)

# Compiler-specific options
if(MSVC)
    target_compile_options(libcache PRIVATE /W4)
else()
    target_compile_options(libcache PRIVATE -Wall -Wextra -Wpedantic)
endif()

# On Linux, set the RPATH for the shared library
if(UNIX)
    set_target_properties(libcache PROPERTIES
        BUILD_WITH_INSTALL_RPATH FALSE
        INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endif()
