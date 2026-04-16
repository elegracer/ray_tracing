#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

OPTIX_add_sample_executable( optixSphere target_name
  optixSphere.cu
  optixSphere.cpp
  optixSphere.h
  )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
