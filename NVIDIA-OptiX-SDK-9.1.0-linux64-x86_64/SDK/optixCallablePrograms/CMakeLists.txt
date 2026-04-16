#
# SPDX-FileCopyrightText: Copyright (c) 2020 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# 

OPTIX_add_sample_executable( optixCallablePrograms target_name
  optixCallablePrograms.cpp
  optixCallablePrograms.cu
  optixCallablePrograms.h
  OPTIONS -rdc true
  )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )

