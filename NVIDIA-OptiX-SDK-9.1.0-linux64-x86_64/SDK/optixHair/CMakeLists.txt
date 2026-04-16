#
# SPDX-FileCopyrightText: Copyright (c) 2020 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# 

OPTIX_add_sample_executable( optixHair target_name
  optixHair.cpp
  optixHair.h
  optixHair.cu
  Renderers.h
  Renderers.cpp
  Hair.h
  Hair.cpp
  Head.h
  Head.cpp
  ProgramGroups.h
  ProgramGroups.cpp
  Util.h
  OPTIONS -rdc true
  )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
