#
# SPDX-FileCopyrightText: Copyright (c) 2020 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# 

OPTIX_add_sample_executable( optixModuleCreateAbort target_name
  optixModuleCreateAbort.cu
  optixModuleCreateAbort_ch.cu
  optixModuleCreateAbort.cpp
  optixModuleCreateAbort.h
  OPTIONS -rdc true
  )
  
target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
  
add_executable( optixModuleCreateProcess
  optixModuleCreateProcess.cpp
  )

target_link_libraries( optixModuleCreateProcess
  glad
  sutil_7_sdk
  ${CUDA_LIBRARIES}
  )

set_property( TARGET optixModuleCreateProcess PROPERTY FOLDER "${OPTIX_IDE_FOLDER}" )

add_dependencies( ${target_name} optixModuleCreateProcess )
