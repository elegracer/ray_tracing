#
# SPDX-FileCopyrightText: Copyright (c) 2023 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# 

OPTIX_add_sample_executable( optixMixSDKs target_name
  optixMixSDKs.cpp
  optixSDK750.cpp
  optixSDKCurrent.cpp
  OPTIONS -rdc true
  )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
