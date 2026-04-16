/*
 * SPDX-FileCopyrightText: Copyright (c) 2021 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <optix.h>

#include <sutil/cuda/BufferView.h>
#include <sutil/cuda/Light.h>
#include <sutil/vec_math.h>


const unsigned int NUM_PAYLOAD_VALUES = 4u;


enum ObjectType
{
    PLANE_OBJECT  = 1,
    CUBE_OBJECT   = 1 << 1,
    VOLUME_OBJECT = 1 << 2,
    ANY_OBJECT = 0xFF,
};


enum RayType
{
    RAY_TYPE_RADIANCE  = 0,
    RAY_TYPE_OCCLUSION = 1,
    RAY_TYPE_COUNT = 2
};


struct LaunchParams
{
    unsigned int             width;
    unsigned int             height;
    unsigned int             subframe_index;
    float4*                  accum_buffer;
    uchar4*                  frame_buffer;
    int                      max_depth;

    float3                   eye;
    float3                   U;
    float3                   V;
    float3                   W;

    sutil::BufferView<sutil::Light> lights;
    float3                   miss_color;
    OptixTraversableHandle   handle;

    // Visbility masks
    unsigned int solid_objects;
    unsigned int volume_object;
};


struct MaterialData
{
    struct Lambert
    {
        float3 base_color;
    };

    struct Volume
    {
        float  opacity; // effectively a scale factor for volume density
    };


    union
    {
        Lambert lambert;
        Volume  volume;
    };
};


struct GeometryData
{
    struct Plane
    {
        float3 normal;
    };

    struct Volume
    {
        void* grid;
    };


    union
    {
        Plane  plane;
        Volume volume;
    };
};


struct HitGroupData
{
    GeometryData geometry_data;
    MaterialData material_data;
};


struct PayloadRadiance
{
    float3 result;
    float  depth;
};


struct PayloadOcclusion
{
};
