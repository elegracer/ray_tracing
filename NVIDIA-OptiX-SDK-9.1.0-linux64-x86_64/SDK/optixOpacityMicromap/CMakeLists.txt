#
# SPDX-FileCopyrightText: Copyright (c) 2022 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# 

OPTIX_add_sample_executable( optixOpacityMicromap target_name
  optixOpacityMicromap.cu
  optixOpacityMicromap.cpp
  optixOpacityMicromap.h
  OPTIONS -rdc true
  )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
