/*
 * SPDX-FileCopyrightText: Copyright (c) 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>

#include "optixCustomCache.h"
#include <sutil/cuda/helpers.h>

extern "C" {
__constant__ Params params;
}

extern "C" __global__ void __raygen__optix_custom_cache()
{
    uint3       launch_index = optixGetLaunchIndex();
    RayGenData* rtData       = (RayGenData*)optixGetSbtDataPointer();
    
    // Direct linear conversion without sRGB mapping
    uchar4 color;
    color.x = static_cast<unsigned char>( rtData->r * 255.0f );
    color.y = static_cast<unsigned char>( rtData->g * 255.0f );
    color.z = static_cast<unsigned char>( rtData->b * 255.0f );
    color.w = 255;
    
    params.image[launch_index.y * params.image_width + launch_index.x] = color;
}
