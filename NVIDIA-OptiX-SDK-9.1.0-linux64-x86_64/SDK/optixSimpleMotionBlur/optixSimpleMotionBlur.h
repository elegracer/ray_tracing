/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

enum RayType
{
    RAY_TYPE_RADIANCE = 0,
    RAY_TYPE_COUNT
};


struct Params
{
    unsigned int width;
    unsigned int height;
    float4*      accum_buffer;
    uchar4*      frame_buffer;
    unsigned int subframe_index;

    float3       eye;
    float3       U;
    float3       V;
    float3       W;

    OptixTraversableHandle handle;
};


struct RayGenData
{
};


struct MissData
{
    float3       color;
    unsigned int pad;
};


struct SphereData
{
    float3 center;
    float  radius;
};


struct HitGroupData
{
    float3     color;

    // For spheres.  In real use case, we would have an abstraction for geom data/ material data
    float3     center;
    float      radius;

    unsigned int pad;

};


