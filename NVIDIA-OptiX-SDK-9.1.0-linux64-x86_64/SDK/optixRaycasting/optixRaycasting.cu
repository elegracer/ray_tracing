/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>

#include "optixRaycasting.h"
#include "optixRaycastingKernels.h"

#include <sutil/cuda/LocalGeometry.h>
#include <sutil/cuda/LocalShading.h>

#include <sutil/vec_math.h>

extern "C" {
__constant__ Params params;
}


extern "C" __global__ void __raygen__from_buffer()
{
    const uint3        idx        = optixGetLaunchIndex();
    const uint3        dim        = optixGetLaunchDimensions();
    const unsigned int linear_idx = idx.z * dim.y * dim.x + idx.y * dim.x + idx.x;

    unsigned int t, nx, ny, nz;
    Ray          ray = params.rays[linear_idx];
    optixTrace( params.handle, ray.origin, ray.dir, ray.tmin, ray.tmax, 0.0f, OptixVisibilityMask( 1 ),
                OPTIX_RAY_FLAG_NONE, RAY_TYPE_RADIANCE, RAY_TYPE_COUNT, RAY_TYPE_RADIANCE, t, nx, ny, nz );

    Hit hit;
    hit.t                   = __uint_as_float( t );
    hit.geom_normal.x       = __uint_as_float( nx );
    hit.geom_normal.y       = __uint_as_float( ny );
    hit.geom_normal.z       = __uint_as_float( nz );
    params.hits[linear_idx] = hit;
}


extern "C" __global__ void __miss__buffer_miss()
{
    optixSetPayload_0( __float_as_uint( -1.0f ) );
    optixSetPayload_1( __float_as_uint( 1.0f ) );
    optixSetPayload_2( __float_as_uint( 0.0f ) );
    optixSetPayload_3( __float_as_uint( 0.0f ) );
}


extern "C" __global__ void __closesthit__buffer_hit()
{
    const float t = optixGetRayTmax();

    HitGroupData*        rt_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    sutil::LocalGeometry geom    = getLocalGeometry( rt_data->triangle_data);

    // Set the hit data
    optixSetPayload_0( __float_as_uint( t ) );
    optixSetPayload_1( __float_as_uint( geom.N.x ) );
    optixSetPayload_2( __float_as_uint( geom.N.y ) );
    optixSetPayload_3( __float_as_uint( geom.N.z ) );
}


extern "C" __global__ void __anyhit__texture_mask()
{
    HitGroupData* rt_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );

    if( rt_data->material_data.alpha_mode == sutil::MaterialData::ALPHA_MODE_MASK )
    {
        sutil::LocalGeometry geom = getLocalGeometry( rt_data->triangle_data );
        float4               mask = sutil::sampleTexture<float4>( rt_data->material_data.pbr.base_color_tex, geom );
        if( mask.w < rt_data->material_data.alpha_cutoff )
        {
            optixIgnoreIntersection();
        }
    }
}

