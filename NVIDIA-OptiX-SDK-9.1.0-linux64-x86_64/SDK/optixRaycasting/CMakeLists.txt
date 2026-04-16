#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# 

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/optixRaycastingKernels.cu
  PROPERTIES CUDA_SOURCE_PROPERTY_FORMAT OBJ
  )

OPTIX_add_sample_executable( optixRaycasting target_name
  optixRaycasting.cu
  optixRaycasting.cpp
  optixRaycasting.h
  optixRaycastingKernels.cu
  optixRaycastingKernels.h
  )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
