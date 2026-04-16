#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/optixNVLink_kernels.cu
  PROPERTIES CUDA_SOURCE_PROPERTY_FORMAT OBJ
  )

# Under windows, look for nvml.lib in the cuda toolkit, and the the general cuda path
if( WIN32 )
  find_file( NVML_LIBRARY
      NO_DEFAULT_PATH
      NAMES nvml.lib
      PATHS ${CUDA_TOOLKIT_ROOT_DIR}/lib/x64 $ENV{CUDA_PATH}/lib/x64
      )
endif()

# Under Unix, look for the libnvidia-ml library
if( UNIX )
  find_library( NVML_LIBRARY libnvidia-ml.so )
endif()

# If an NVML library was found, compile using it
if( NVML_LIBRARY )
  set( OPTIX_USE_NVML ON )
endif()

# Set the value of OPTIX_USE_NVML in nvml_configure.h
configure_file( nvml_configure.h.in nvml_configure.h )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

# Set up source files for executable
OPTIX_add_sample_executable( optixNVLink target_name
  optixNVLink_kernels.cu
  optixNVLink.cu
  optixNVLink.cpp
  optixNVLink.h

  ${CMAKE_CURRENT_BINARY_DIR}/nvml_configure.h
  )
set_property( TARGET ${target_name} PROPERTY CUDA_SEPARABLE_COMPILATION ON )

# Set up libraries to link in
# Not linking NVML_LIBRARY, since it will be loaded at runtime
target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
