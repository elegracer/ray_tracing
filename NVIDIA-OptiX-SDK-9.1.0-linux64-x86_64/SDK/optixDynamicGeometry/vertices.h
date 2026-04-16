/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cuda.h>
#include <sutil/vec_math.h>

enum AnimationMode
{
    AnimationMode_None,
    AnimationMode_Deform,
    AnimationMode_Explode
};

extern "C" __host__ void generateAnimatedVetrices( float3* out_vertices, AnimationMode animation_mode, float time, int width, int height );
