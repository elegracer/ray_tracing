#
# SPDX-FileCopyrightText: Copyright (c) 2021 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

# Tag the following kernel to be compiled to OBJ intead of PTX (which is the default for
# OPTIX_add_sample_executable). Must be called before OPTIX_add_sample_executable.
set_source_files_properties(
  ${CMAKE_CURRENT_SOURCE_DIR}/vertices.cu
  PROPERTIES CUDA_SOURCE_PROPERTY_FORMAT OBJ
)

OPTIX_add_sample_executable( optixMotionGeometry target_name
  optixMotionGeometry.cu
  optixMotionGeometry.cpp
  optixMotionGeometry.h
  motionHelper.hpp
  tiny_obj_loader.h
  vertices.cu
  # OPTIONS -rdc true
  )

target_link_libraries( ${target_name} ${CUDA_LIBRARIES} )
