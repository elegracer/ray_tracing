/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>

#include <sutil/cuda/camera.h>
#include <sutil/cuda/geometry.h>
#include <sutil/vec_math.h>

#include "optixCallablePrograms.h"

extern "C" {
__constant__ LaunchParams params;
}


__forceinline__ __device__ void setPayloadResult( float3 p )
{
    optixSetPayload_0( __float_as_uint( p.x ) );
    optixSetPayload_1( __float_as_uint( p.y ) );
    optixSetPayload_2( __float_as_uint( p.z ) );
}


__forceinline__ __device__ void setPayloadOcclusion( float attenuation )
{
    optixSetPayload_0( __float_as_uint( attenuation ) );
}


//------------------------------------------------------------------------------
//
// Direct callables for shading
//
//------------------------------------------------------------------------------

extern "C" __device__ float3 __direct_callable__phong_shade( float3 hit_point, float3 ray_dir, float3 normal )
{
    constexpr float3 Ka        = { 0.2f, 0.5f, 0.5f };
    constexpr float3 Kd        = { 0.2f, 0.7f, 0.8f };
    constexpr float3 Ks        = { 0.9f, 0.9f, 0.9f };
    constexpr float  phong_exp = 64.0f;

    float3 result = make_float3( 0.0f );

    for( int i = 0; i < params.lights.count; ++i )
    {
        sutil::Light light = params.lights[i];
        if( light.type == sutil::Light::Type::POINT )
        {
            // compute direct lighting
            float  Ldist = length( light.point.position - hit_point );
            float3 L     = normalize( light.point.position - hit_point );
            float  nDl   = dot( normal, L );

            result += Kd * nDl * light.point.color;

            float3 H   = normalize( L - ray_dir );
            float  nDh = dot( normal, H );
            if( nDh > 0 )
            {
                float power = pow( nDh, phong_exp );
                result += Ks * power * light.point.color;
            }
        }
        else if( light.type == sutil::Light::Type::AMBIENT )
        {
            // ambient contribution
            result += Ka * light.ambient.color;
        }
    }

    return result;
}


extern "C" __device__ float3 __direct_callable__checkered_shade(
    float3 hit_point,
    float3 ray_dir,
    float3 normal
)
{
    float3 result;

    float value = dot( normal, ray_dir );
    if( value < 0 )
    {
        value *= -1;
    }

    const float3 sphere_normal = normalize( hit_point );
    const float  a             = acos( sphere_normal.y );
    const float  b             = atan2( sphere_normal.x, sphere_normal.z ) + M_PIf;
    sutil::Light::Ambient light         = params.lights[0].ambient;
    if( ( fmod( a, M_PIf / 8 ) < M_PIf / 16 ) ^ ( fmod( b, M_PIf / 4 ) < M_PIf / 8 ) )
    {
        result = light.color + ( value * make_float3( 0.0f ) );
    }
    else
    {
        result = light.color + ( value * make_float3( 1.0f ) );
    }

    return clamp( result, 0.0f, 1.0f );
}


extern "C" __device__ float3 __direct_callable__normal_shade( float3 hit_point, float3 ray_dir, float3 normal )
{
    return normalize( normal ) * 0.5f + 0.5f;
}


// Continuation callable for background
extern "C" __device__ float3 __continuation_callable__raydir_shade( float3 ray_dir )
{
    return normalize( ray_dir ) * 0.5f + 0.5f;
}


//-------------------------------------------------------------------------------------------------
//
// Rendering programs
//
//-------------------------------------------------------------------------------------------------

extern "C" __global__ void __raygen__pinhole()
{
    sutil::pinholeGenerateRay(
        params.subframe_index,
        params.accum_buffer,
        params.frame_buffer,
        params.eye,
        params.U,
        params.V,
        params.W,
        RAY_TYPE_RADIANCE,
        RAY_TYPE_COUNT,
        params.handle
    );
}

// Closest hit
extern "C" __global__ void __closesthit__radiance()
{
    const HitGroupData* hitgroup_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );

    const float3 ray_orig  = optixGetWorldRayOrigin();
    const float3 ray_dir   = optixGetWorldRayDirection();
    const float  ray_t     = optixGetRayTmax();
    const float3 hit_point = ray_orig + ray_t * ray_dir;

    const float3 object_normal = make_float3(
        __uint_as_float( optixGetAttribute_0() ),
        __uint_as_float( optixGetAttribute_1() ),
        __uint_as_float( optixGetAttribute_2() )
    );
    const float3 world_normal = normalize( optixTransformNormalFromObjectToWorldSpace( object_normal ) );
    const float3 ffnormal     = faceforward( world_normal, -ray_dir, world_normal );

    // Use a direct callable to set the result
    float3 result = optixDirectCall<float3, float3, float3, float3>( hitgroup_data->dc_index, hit_point, ray_dir, ffnormal );
    setPayloadResult( result );
}

// Miss
extern "C" __global__ void __miss__raydir_shade()
{
    const float3 ray_dir = optixGetWorldRayDirection();

    float3 result = optixContinuationCall<float3, float3>( 0, ray_dir );
    setPayloadResult( result );
}


extern "C" __global__ void __intersection__sphere()
{
    const HitGroupData* hit_group_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    sutil::intersectSphere( hit_group_data->sphere.center, hit_group_data->sphere.radius );
}


