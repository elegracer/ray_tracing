#
# SPDX-FileCopyrightText: Copyright (c) 2010 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

# See top level CMakeLists.txt file for documentation of OPTIX_add_sample_executable.


OPTIX_add_sample_executable( optixDenoiser target_name
    OptiXDenoiser.h
    optixDenoiser.cpp
    )

set_target_properties( ${target_name} PROPERTIES CXX_STANDARD 14 )

target_link_libraries( ${target_name} miniz
  ${CUDA_LIBRARIES}
  )


