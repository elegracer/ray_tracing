/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

//
// Kernels for processing hits and rays outside of OptiX
//

struct Ray
{
    float3 origin;
    float  tmin;
    float3 dir;
    float  tmax;
};

struct Hit
{
    float  t;
    float3 geom_normal;
};

void createRaysOrthoOnDevice( Ray* rays_device, int width, int height, float3 bbmin, float3 bbmax, float padding );

void translateRaysOnDevice( Ray* rays_device, int count, float3 offset );

void shadeHitsOnDevice( float3* image_device, int count, const Hit* hits_device );

