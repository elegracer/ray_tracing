#
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

OPTIX_add_sample_executable( optixCompileCancel target_name
  optixCompileCancel.cpp
  optixCompileCancel.cu
  optixCompileCancel.h
  OPTIONS -rdc true
)

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
)
