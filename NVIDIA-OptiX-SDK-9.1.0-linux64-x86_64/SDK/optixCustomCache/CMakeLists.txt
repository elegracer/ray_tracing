#
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

# Build libcache shared library
add_subdirectory(libcache)

OPTIX_add_sample_executable( optixCustomCache target_name
  "${SAMPLES_DIR}/lib/CompileWithTasks.h"
  optixCustomCache.cu
  optixCustomCache.cpp
  optixCustomCache.h

  OPTIONS -rdc true
  )

include_directories( "${SAMPLES_DIR}/lib" )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )

# Add libcache include directory for DllDatabaseTypes.h
target_include_directories( ${target_name} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/libcache
  )

# On Unix, link against dl for dynamic loading
if(UNIX)
  target_link_libraries( ${target_name} ${CMAKE_DL_LIBS} )
endif()

# Add dependency on libcache
add_dependencies( ${target_name} libcache )

# Copy cache_server.py next to the executable for easy access & testing
add_custom_command( TARGET ${target_name} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/netcache/cache_server.py
          $<TARGET_FILE_DIR:${target_name}>
  COMMENT "Copying cache_server.py to runtime output directory"
)
