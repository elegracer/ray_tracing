#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

OPTIX_add_sample_executable( optixWhitted target_name
  optixWhitted.cpp
  optixWhitted.cu
  optixWhitted.h
  )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
