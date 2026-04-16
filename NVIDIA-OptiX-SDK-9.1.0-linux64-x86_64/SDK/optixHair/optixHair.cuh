/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/cuda/GeometryData.h>
#include <sutil/cuda/Light.h>
#include <sutil/cuda/MaterialData.h>


constexpr unsigned int NUM_ATTRIBUTE_VALUES = 4u;


enum RayType
{
    RAY_TYPE_RADIANCE  = 0,
    RAY_TYPE_OCCLUSION = 1,
    RAY_TYPE_COUNT = 2
};


struct PayloadRadiance
{
    float3 result;
    float  importance;
};


struct PayloadOcclusion
{
    float3 result;
};


struct HitGroupData
{
    sutil::GeometryData geometry_data;
    sutil::MaterialData material_data;
};


struct LaunchParams
{
    unsigned int                       width;
    unsigned int                       height;
    unsigned int                       subframe_index;
    float4*                            accum_buffer;
    uchar4*                            frame_buffer;
    int                                max_depth;
    float                              scene_epsilon;

    float3                             eye;
    float3                             U;
    float3                             V;
    float3                             W;

    sutil::BufferView<sutil::Light>    lights;
    float3                             miss_color;
    OptixTraversableHandle             handle;
};
