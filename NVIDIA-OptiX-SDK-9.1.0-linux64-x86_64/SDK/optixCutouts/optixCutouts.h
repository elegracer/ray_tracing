/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <optix.h>

#include <sutil/cuda/GeometryData.h>
#include <sutil/cuda/MaterialData.h>
#include <sutil/vec_math.h>


constexpr float         CIRCLE_RADIUS = 0.65f;
constexpr unsigned int  NUM_ATTRIBUTE_VALUES = 4u;

enum RayType
{
    RAY_TYPE_RADIANCE = 0,
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
    float4*      accum_buffer;
    uchar4*      frame_buffer;
    unsigned int width;
    unsigned int height;
    unsigned int samples_per_launch;
    unsigned int subframe_index;

    float3       eye;
    float3       U;
    float3       V;
    float3       W;

    ParallelogramLight     light; // TODO: make light list
    OptixTraversableHandle handle;
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

    sutil::Sphere       sphere;
    sutil::MaterialData material_data;

    float3   emission_color;
    float3   diffuse_color;
    float4*  vertices;
    float2*  tex_coords;
};
