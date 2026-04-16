/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>

#include "optixOpacityMicromap.h"

#include <sutil/cuda/helpers.h>
#include <sutil/vec_math.h>

extern "C" {
__constant__ Params params;
}


static __forceinline__ __device__ void setPayloadColor( float3 p )
{
    optixSetPayload_0( __float_as_uint( p.x ) );
    optixSetPayload_1( __float_as_uint( p.y ) );
    optixSetPayload_2( __float_as_uint( p.z ) );
}

static __forceinline__ __device__ void setPayloadAnyhit( unsigned int a )
{
    optixSetPayload_3( a );
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


extern "C" __global__ void __raygen__rg()
{
    // Lookup our location within the launch grid
    const uint3 idx = optixGetLaunchIndex();
    const uint3 dim = optixGetLaunchDimensions();

    // Map our launch idx to a screen location and create a ray from the camera
    // location through the screen
    float3 ray_origin, ray_direction;
    computeRay( idx, dim, ray_origin, ray_direction );

    // Trace the ray against our scene hierarchy
    unsigned int p0, p1, p2, p3=0;
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
            p0, p1, p2, p3 );
    float3 result;
    result.x = __uint_as_float( p0 );
    result.y = __uint_as_float( p1 );
    result.z = __uint_as_float( p2 );
    unsigned int anyhit_executed = p3;

    // If anyhit was executed, tint the pixel towards white
    if( anyhit_executed )
        result = lerp( result, make_float3( 1.0f), 0.075f );

    // Record results in our output raster
    params.image[idx.y * params.image_width + idx.x] = sutil::make_color( result );
}


extern "C" __global__ void __miss__ms()
{
    MissData* miss_data  = reinterpret_cast<MissData*>( optixGetSbtDataPointer() );
    setPayloadColor(  miss_data->bg_color );
}


extern "C" __global__ void __closesthit__ch()
{
    // When built-in triangle intersection is used, a number of fundamental
    // attributes are provided by the OptiX API, including barycentric coordinates.
    const float2 barycentrics = optixGetTriangleBarycentrics();

    setPayloadColor( make_float3( barycentrics*0.5f, 0.5f ) );
}


extern "C" __global__ void __anyhit__opacity()
{
    setPayloadAnyhit( 1u ); // Register that anyhit was invoked

    const HitGroupData* rt_data      = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    const float2        barycentrics = optixGetTriangleBarycentrics();
    const int           prim_idx     = optixGetPrimitiveIndex();

    const float2 uv0 = rt_data->uvs[ prim_idx*3 + 0 ];
    const float2 uv1 = rt_data->uvs[ prim_idx*3 + 1 ];
    const float2 uv2 = rt_data->uvs[ prim_idx*3 + 2 ];

    const float2 uv = computeUV( barycentrics, uv0, uv1, uv2 );

    if( inCircle( uv ) )
        optixIgnoreIntersection();
}

