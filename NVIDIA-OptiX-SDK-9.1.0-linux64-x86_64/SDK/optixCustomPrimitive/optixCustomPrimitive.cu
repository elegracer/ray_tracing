/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>

#include "optixCustomPrimitive.h"
#include <sutil/cuda/geometry.h>
#include <sutil/cuda/helpers.h>
#include <sutil/vec_math.h>

extern "C" {
__constant__ Params params;
}


static __forceinline__ __device__ void trace(
        OptixTraversableHandle handle,
        float3                 ray_origin,
        float3                 ray_direction,
        float                  tmin,
        float                  tmax,
        float3*                prd
        )
{
    unsigned int p0, p1, p2;
    p0 = __float_as_int( prd->x );
    p1 = __float_as_int( prd->y );
    p2 = __float_as_int( prd->z );
    optixTrace(
            handle,
            ray_origin,
            ray_direction,
            tmin,
            tmax,
            0.0f,                // rayTime
            OptixVisibilityMask( 1 ),
            OPTIX_RAY_FLAG_NONE,
            0,                   // SBT offset
            RAY_TYPE_COUNT,      // SBT stride
            0,                   // missSBTIndex
            p0, p1, p2 );
    prd->x = __int_as_float( p0 );
    prd->y = __int_as_float( p1 );
    prd->z = __int_as_float( p2 );
}


static __forceinline__ __device__ void setPayload( float3 p )
{
    optixSetPayload_0( __float_as_int( p.x ) );
    optixSetPayload_1( __float_as_int( p.y ) );
    optixSetPayload_2( __float_as_int( p.z ) );
}


static __forceinline__ __device__ float3 getPayload()
{
    return make_float3(
            __int_as_float( optixGetPayload_0() ),
            __int_as_float( optixGetPayload_1() ),
            __int_as_float( optixGetPayload_2() )
            );
}


extern "C" __global__ void __raygen__rg()
{
    const uint3 idx = optixGetLaunchIndex();
    const uint3 dim = optixGetLaunchDimensions();

    const RayGenData* rtData = (RayGenData*)optixGetSbtDataPointer();
    const float3      U      = rtData->camera_u;
    const float3      V      = rtData->camera_v;
    const float3      W      = rtData->camera_w;
    const float2      d = 2.0f * make_float2(
            static_cast<float>( idx.x ) / static_cast<float>( dim.x ),
            static_cast<float>( idx.y ) / static_cast<float>( dim.y )
            ) - 1.0f;

    const float3 origin      = rtData->cam_eye;
    const float3 direction   = normalize( d.x * U + d.y * V + W );
    float3       payload_rgb = make_float3( 0.5f, 0.5f, 0.5f );
    trace( params.handle,
            origin,
            direction,
            0.00f,  // tmin
            1e16f,  // tmax
            &payload_rgb );

    params.image[idx.y * params.image_width + idx.x] = sutil::make_color( payload_rgb );
}


extern "C" __global__ void __miss__ms()
{
    MissData* rt_data  = reinterpret_cast<MissData*>( optixGetSbtDataPointer() );
    float3    payload = getPayload();
    setPayload( make_float3( rt_data->r, rt_data->g, rt_data->b ) );
}


extern "C" __global__ void __closesthit__ch()
{
    const float3 shading_normal =
        make_float3(
                __int_as_float( optixGetAttribute_0() ),
                __int_as_float( optixGetAttribute_1() ),
                __int_as_float( optixGetAttribute_2() )
                );
    setPayload( normalize( optixTransformNormalFromObjectToWorldSpace( shading_normal ) ) * 0.5f + 0.5f );
}

extern "C" __global__ void __intersection__sphere()
{
    const HitGroupData* hit_group_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    sutil::intersectSphere( hit_group_data->sphere.center, hit_group_data->sphere.radius );
}


