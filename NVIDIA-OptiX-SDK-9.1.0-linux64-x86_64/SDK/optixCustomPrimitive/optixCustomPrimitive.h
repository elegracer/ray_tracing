/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/cuda/GeometryData.h>
#include <sutil/cuda/MaterialData.h>


constexpr unsigned int NUM_ATTRIBUTE_VALUES = 4u;



enum RayType
{
    RAY_TYPE_RADIANCE = 0,
    RAY_TYPE_COUNT
};


struct HitGroupData
{
    sutil::Sphere       sphere;
    sutil::MaterialData material_data;
};


struct Params
{
    uchar4*                image;
    unsigned int           image_width;
    unsigned int           image_height;
    int                    origin_x;
    int                    origin_y;
    OptixTraversableHandle handle;
};


struct RayGenData
{
    float3 cam_eye;
    float3 camera_u, camera_v, camera_w;
};


struct MissData
{
    float r, g, b;
};

