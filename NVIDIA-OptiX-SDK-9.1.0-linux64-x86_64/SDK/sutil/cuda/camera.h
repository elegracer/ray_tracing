/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vector_types.h>
#include <optix.h>

#include <sutil/cuda/helpers.h>
#include <sutil/cuda/random.h>

namespace sutil
{

__device__ __forceinline__ void pinholeGenerateRay(
    unsigned int           subframe_index,
    float4*                accum_buffer,
    uchar4*                frame_buffer,
    float3                 eye,
    float3                 U,
    float3                 V,
    float3                 W,
    unsigned int           ray_type_radiance,
    unsigned int           ray_type_count,
    OptixTraversableHandle handle
)
{
    const uint3 idx = optixGetLaunchIndex();
    const uint3 dim = optixGetLaunchDimensions();

    // Generate a sample seed for this pixel at this subframe
    const unsigned int image_index = dim.x*idx.y + idx.x;
    unsigned int       seed        = tea<16>( image_index, subframe_index );

    // Subpixel jitter: send the ray through a different position inside the pixel each time,
    // to provide antialiasing. The center of each pixel is at fraction (0.5,0.5)
    const float2 subpixel_jitter =
        subframe_index == 0       ?
        make_float2( 0.5f, 0.5f ) :
        make_float2( rnd( seed ), rnd( seed ) );

    // The direction from eye to screen in camera space
    const float2 d = ( ( make_float2( idx.x, idx.y ) + subpixel_jitter ) / make_float2( dim.x, dim.y) )*2.f - 1.f;

    // Now move to world space
    const float3 ray_origin    = eye;
    const float3 ray_direction = normalize( d.x*U + d.y*V + W );

    // Ray payload
    float3 result{};
    float  importance = 1.0f;

    // Trace ray from eye through our sampled screen coordinate
    optixTrace(
        handle,                           // Optix traversable handle
        ray_origin, ray_direction,
        1e-4f,                            // tmin
        1e16f,                            // tmax
        0.0f,                             // time
        OptixVisibilityMask( 1 ),
        OPTIX_RAY_FLAG_NONE,
        ray_type_radiance,                // SBT offset
        ray_type_count,                   // SBT stride
        ray_type_radiance,                // SBT miss index
        float3_as_args( result ),                     // Payload: output result color
        reinterpret_cast<unsigned int&>( importance ) // Payload: input current ray importance
    );

    // Deposit result in accumulation buffer and convert accumulated value to discretized color
    float4 acc_val = accum_buffer[image_index];
    if( subframe_index > 0 )
    {
        const float t = 1.0f / static_cast<float>( subframe_index + 1 );
        acc_val       = lerp( acc_val, make_float4( result, 0.0f ), t );
    }
    else
    {
        acc_val = make_float4( result, 0.0f );
    }
    frame_buffer[image_index] = make_color( acc_val );
    accum_buffer[image_index] = acc_val;
}

} // namespace sutil
