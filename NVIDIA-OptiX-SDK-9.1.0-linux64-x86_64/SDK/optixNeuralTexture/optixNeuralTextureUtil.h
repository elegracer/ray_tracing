/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/vec_math.h>

#include "optixNeuralTexture.h"

extern "C" {
__constant__ LaunchParams params;
}

//------------------------------------------------------------------------------
//
// GGX/smith shading helpers
//
//------------------------------------------------------------------------------


__device__ __forceinline__ float3 schlick( const float3 spec_color, const float V_dot_H )
{
    return spec_color + ( make_float3( 1.0f ) - spec_color ) * powf( 1.0f - V_dot_H, 5.0f );
}


__device__ __forceinline__ float vis( const float N_dot_L, const float N_dot_V, const float alpha )
{
    const float alpha_sq = alpha*alpha;

    const float ggx0 = N_dot_L * sqrtf( N_dot_V*N_dot_V * ( 1.0f - alpha_sq ) + alpha_sq );
    const float ggx1 = N_dot_V * sqrtf( N_dot_L*N_dot_L * ( 1.0f - alpha_sq ) + alpha_sq );

    return 2.0f * N_dot_L * N_dot_V / (ggx0+ggx1);
}


__device__ __forceinline__ float ggxNormal( const float N_dot_H, const float alpha )
{
    const float alpha_sq   = alpha*alpha;
    const float N_dot_H_sq = N_dot_H*N_dot_H;
    const float x          = N_dot_H_sq*( alpha_sq - 1.0f ) + 1.0f;
    return alpha_sq/( M_PIf*x*x );
}


__device__ __forceinline__ float3 linearize( float3 c )
{
    return make_float3(
            powf( c.x, 2.2f ),
            powf( c.y, 2.2f ),
            powf( c.z, 2.2f )
            );
}


//------------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------------


static __forceinline__ __device__ void traceRadiance(
        OptixTraversableHandle      handle,
        float3                      ray_origin,
        float3                      ray_direction,
        float                       tmin,
        float                       tmax,
        PayloadRadiance*   payload
        )
{
    unsigned int u0 = 0; // output only
    unsigned int u1 = 0; // output only
    unsigned int u2 = 0; // output only
    optixTrace(
            handle,
            ray_origin, ray_direction,
            tmin,
            tmax,
            0.0f,                     // rayTime
            OptixVisibilityMask( 1 ),
            OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            RAY_TYPE_RADIANCE,        // SBT offset
            RAY_TYPE_COUNT,           // SBT stride
            RAY_TYPE_RADIANCE,        // missSBTIndex
            u0, u1, u2 );

     payload->result.x = __uint_as_float( u0 );
     payload->result.y = __uint_as_float( u1 );
     payload->result.z = __uint_as_float( u2 );
}


static __forceinline__ __device__ float traceOcclusion(
        OptixTraversableHandle handle,
        float3                 ray_origin,
        float3                 ray_direction,
        float                  tmin,
        float                  tmax
        )
{
    // Introduce the concept of 'pending' and 'committed' attenuation.
    // This avoids the usage of closesthit shaders and allows the usage of the OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT flag.
    // The attenuation is marked as pending with a positive sign bit and marked committed by switching the sign bit.
    // Attenuation magnitude can be changed in anyhit programs and stays pending.
    // The final attenuation gets committed in the miss shader (by setting the sign bit).
    // If no miss shader is invoked (traversal was terminated due to an opaque hit)
    // the attenuation is not committed and the ray is deemed fully occluded.
    unsigned int attenuation = __float_as_uint(1.f);
    optixTrace(
            handle,
            ray_origin,
            ray_direction,
            tmin,
            tmax,
            0.0f,                    // rayTime
            OptixVisibilityMask( 1 ),
            OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT | OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT,
            RAY_TYPE_OCCLUSION,      // SBT offset
            RAY_TYPE_COUNT,          // SBT stride
            RAY_TYPE_OCCLUSION,      // missSBTIndex
            attenuation );

    // committed attenuation is negated
    return fmaxf(0, -__uint_as_float(attenuation));
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


__forceinline__ __device__ void setPayloadOcclusionCommit()
{
    // set the sign
    optixSetPayload_0( optixGetPayload_0() | 0x80000000 );
}
