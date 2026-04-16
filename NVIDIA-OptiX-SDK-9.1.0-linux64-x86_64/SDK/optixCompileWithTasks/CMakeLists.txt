#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# 

OPTIX_add_sample_executable( optixCompileWithTasks target_name
    "${SAMPLES_DIR}/lib/CompileWithTasks.h"
    optixCompileWithTasks.cpp
    )

include_directories( "${SAMPLES_DIR}/lib" )
target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
