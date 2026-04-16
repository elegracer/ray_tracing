/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

enum RayType
{
    RAY_TYPE_RADIANCE  = 0,
    RAY_TYPE_OCCLUSION = 1,
    RAY_TYPE_COUNT
};


struct ParallelogramLight
{
    float3 corner;
    float3 v1, v2;
    float3 normal;
    float3 emission;
};


struct Params
{
    unsigned int subframe_index; 
        
    int2*        sample_index_buffer;
    float4*      sample_accum_buffer;
    uchar4*      result_buffer;
    unsigned int width;
    unsigned int height;
    unsigned int samples_per_launch;
    unsigned int device_idx;

    float3       eye;
    float3       U;
    float3       V;
    float3       W;

    ParallelogramLight     light; 
    OptixTraversableHandle handle;

    float        device_color_scale; // to turn on/off multi-gpu pattern overlay
};


struct RayGenData
{
    float r, g, b;
};


struct MissData
{
    float r, g, b;
};


struct HitGroupData
{
    float3  emission_color;
    float3  diffuse_color;
    float4* vertices;
    float2* tex_coords;
    cudaTextureObject_t diffuse_texture;
};
