/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>

#include "optixRibbons.h"

#include <sutil/cuda/helpers.h>
#include <sutil/vec_math.h>



extern "C" {
__constant__ Params params;
}


static __forceinline__ __device__ void setPayload( float3 p )
{
    optixSetPayload_0( __float_as_uint( p.x ) );
    optixSetPayload_1( __float_as_uint( p.y ) );
    optixSetPayload_2( __float_as_uint( p.z ) );
}


static __forceinline__ __device__ void computeRay( uint3 idx, uint3 dim, float3& origin, float3& direction )
{
    const float3 U = params.cam_u;
    const float3 V = params.cam_v;
    const float3 W = params.cam_w;
    const float2 d = 2.0f * make_float2(
            static_cast<float>( idx.x ) / static_cast<float>( dim.x ),
            static_cast<float>( idx.y ) / static_cast<float>( dim.y )
            ) - 1.0f;

    origin    = params.cam_eye;
    direction = normalize( d.x * U + d.y * V + W );
}


extern "C" __global__ void __raygen__basic()
{
    // Lookup our location within the launch grid
    const uint3 idx = optixGetLaunchIndex();
    const uint3 dim = optixGetLaunchDimensions();

    // Map our launch idx to a screen location and create a ray from the camera
    // location through the screen
    float3 ray_origin, ray_direction;
    computeRay( idx, dim, ray_origin, ray_direction );

    // Trace the ray against our scene hierarchy
    unsigned int p0, p1, p2;
    optixTrace(
            params.handle,
            ray_origin,
            ray_direction,
            0.0f,                // Min intersection distance
            1e16f,               // Max intersection distance
            0.0f,                // rayTime -- used for motion blur
            OptixVisibilityMask( 255 ), // Specify always visible
            OPTIX_RAY_FLAG_NONE,
            0,                   // SBT offset   -- See SBT discussion
            RAY_TYPE_COUNT,      // SBT stride   -- See SBT discussion
            0,                   // missSBTIndex -- See SBT discussion
            p0, p1, p2 );
    float3 result;
    result.x = __uint_as_float( p0 );
    result.y = __uint_as_float( p1 );
    result.z = __uint_as_float( p2 );

    // Record results in our output raster
    params.image[idx.y * params.image_width + idx.x] = sutil::make_color( result );
}

extern "C" __global__ void __miss__ms()
{
    MissData* miss_data  = reinterpret_cast<MissData*>( optixGetSbtDataPointer() );
    setPayload(  miss_data->bg_color );
}


extern "C" __global__ void __closesthit__ch()
{
    // When built-in ribbon intersection is used, the curve parameters u and v are provided
    // by the OptiX API using optixGetRibbonParameters.
    // The u range is [0,1] over the curve segment, v is [-1,1] over the width.
    // The geometric normal at the intersection position is provided by optixGetRibbonNormal.

    const unsigned int           prim_idx    = optixGetPrimitiveIndex();
    const OptixTraversableHandle gas         = optixGetGASTraversableHandle();
    const unsigned int           sbtGASIndex = optixGetSbtGASIndex();
    const float2                 uv          = optixGetRibbonParameters();
    float3                       obj_normal  = optixGetRibbonNormal( gas, prim_idx, sbtGASIndex, 0.f /*time*/, uv );

    float3 world_normal = normalize( optixTransformNormalFromObjectToWorldSpace( obj_normal ) );
    // 2-sided ribbon surface
    world_normal = faceforward( world_normal, -optixGetWorldRayDirection(), world_normal );

    setPayload( make_float3( world_normal.x * 0.5f + 0.5f,
                             world_normal.y * 0.5f + 0.5f,
                             world_normal.z * 0.5f + 0.5f ) );
}
