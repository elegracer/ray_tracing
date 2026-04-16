/*
 * SPDX-FileCopyrightText: Copyright (c) 2021 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "volume.h"

#include <cuda_runtime.h>
#include "nanovdb/NanoVDB.h"


static __forceinline__ __device__ void traceRadiance(
	OptixTraversableHandle handle,
	float3                 ray_origin,
	float3                 ray_direction,
	float                  tmin,
	float                  tmax,
	unsigned int           mask,
	PayloadRadiance* payload )
{
    unsigned int u0=0, u1=0, u2=0, u3=0;
    optixTrace(
            handle,
            ray_origin, ray_direction,
            tmin,
            tmax,
            0.0f,                     // rayTime
            mask,
            OPTIX_RAY_FLAG_NONE,
            RAY_TYPE_RADIANCE,        // SBT offset
            RAY_TYPE_COUNT,           // SBT stride
            RAY_TYPE_RADIANCE,        // missSBTIndex
            u0, u1, u2, u3 );

     payload->result.x = __uint_as_float( u0 );
     payload->result.y = __uint_as_float( u1 );
     payload->result.z = __uint_as_float( u2 );
     payload->depth    = __uint_as_float( u3 );
}


static __forceinline__ __device__ void traceOcclusion(
        OptixTraversableHandle handle,
        float3                 ray_origin,
        float3                 ray_direction,
        float                  tmin,
        float                  tmax,
        unsigned int           mask,
        float*                 transmittance
        )
{
    optixTrace(
        handle,
        ray_origin,
        ray_direction,
        tmin,
        tmax,
        0.0f,  // rayTime
        mask,
        OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT,  //OPTIX_RAY_FLAG_NONE,
        RAY_TYPE_OCCLUSION,      // SBT offset
        RAY_TYPE_COUNT,          // SBT stride
        RAY_TYPE_OCCLUSION,      // missSBTIndex
        reinterpret_cast<unsigned int&>( *transmittance )
    );
}
