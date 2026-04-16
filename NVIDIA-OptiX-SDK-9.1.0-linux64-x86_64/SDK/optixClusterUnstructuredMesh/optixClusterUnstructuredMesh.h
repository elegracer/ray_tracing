/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <optix.h>


struct Params
{
    uchar4*                frameBuffer;
    unsigned int           width;
    unsigned int           height;
    float3                 eye, U, V, W;
    OptixTraversableHandle handle;
    int                    subframeIndex;
};


struct RayGenData
{
};


struct MissData
{
    float4 bg_color;
};


struct HitGroupData
{
};
