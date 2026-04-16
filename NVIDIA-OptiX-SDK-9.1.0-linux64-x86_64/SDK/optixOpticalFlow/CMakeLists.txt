#
# SPDX-FileCopyrightText: Copyright (c) 2010 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

# See top level CMakeLists.txt file for documentation of OPTIX_add_sample_executable.

if( NOT CUDA_CUDA_LIBRARY )
  return()
endif()

set_source_files_properties( optixOpticalFlow.cu PROPERTIES CUDA_SOURCE_PROPERTY_FORMAT OBJ )

OPTIX_add_sample_executable( optixOpticalFlow target_name
    optixOpticalFlow.cpp
    optixOpticalFlow.cu
    nvOpticalFlowCommon.h
    nvOpticalFlowCuda.h
    )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  ${CUDA_CUDA_LIBRARY}
  ${CMAKE_DL_LIBS}
  )


