#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# 

# Tag the following kernel to be compiled to OBJ intead of PTX (which is the default for
# OPTIX_add_sample_executable). Must be called before OPTIX_add_sample_executable.
set_source_files_properties(
  ${CMAKE_CURRENT_SOURCE_DIR}/clusterUtils.cu
  PROPERTIES CUDA_SOURCE_PROPERTY_FORMAT OBJ
)


OPTIX_add_sample_executable( optixClusterStructuredMesh target_name
  optixClusterStructuredMesh.cu
  optixClusterStructuredMesh.cpp
  optixClusterStructuredMesh.h
  clusterUtils.cu
  clusterUtils.h
  # OPTIONS -rdc true
  )


target_link_libraries( ${target_name} 
  ${CUDA_LIBRARIES}
)
