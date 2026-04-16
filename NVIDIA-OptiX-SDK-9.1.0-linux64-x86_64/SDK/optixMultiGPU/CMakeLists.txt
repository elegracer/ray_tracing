#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/optixMultiGPU_kernels.cu
  PROPERTIES CUDA_SOURCE_PROPERTY_FORMAT OBJ
  )

OPTIX_add_sample_executable( optixMultiGPU target_name
  optixMultiGPU_kernels.cu
  optixMultiGPU.cu
  optixMultiGPU.cpp
  optixMultiGPU.h
  )
set_property(TARGET ${target_name} PROPERTY CUDA_SEPARABLE_COMPILATION ON)

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
