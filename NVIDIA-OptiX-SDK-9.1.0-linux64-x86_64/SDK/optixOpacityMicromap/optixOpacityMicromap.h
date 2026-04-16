/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/vec_math.h>

constexpr float CIRCLE_RADIUS = 0.75f;

enum RayType
{
    RAY_TYPE_RADIANCE = 0,
    RAY_TYPE_COUNT
};

//-----------------------------------------------------------------------------
//
// Helper functions to be used when pre-baking opacity into OMM and when 
// evaluating opacity within anyhit function
//
//-----------------------------------------------------------------------------

static __host__ __device__ __inline__ float2 computeUV( float2 bary, float2 uv0, float2 uv1, float2 uv2 )
{
    return ( 1.0f - bary.x - bary.y )*uv0 + bary.x*uv1 + bary.y*uv2;
}

static __host__ __device__ __inline__ bool inCircle( const float2 uv )
{
    return ( uv.x * uv.x + uv.y * uv.y ) < ( CIRCLE_RADIUS * CIRCLE_RADIUS );
};


//-----------------------------------------------------------------------------
//
// Types
//
//-----------------------------------------------------------------------------
struct Params
{
    uchar4*                image;
    unsigned int           image_width;
    unsigned int           image_height;
    float3                 cam_eye;
    float3                 cam_u, cam_v, cam_w;
    OptixTraversableHandle handle;
};


struct RayGenData
{
    // No data needed
};


struct MissData
{
    float3 bg_color;
};


struct HitGroupData
{
    float2* uvs;
};
