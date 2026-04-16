/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/cuda/GeometryData.h>
#include <sutil/cuda/Light.h>
#include <sutil/cuda/MaterialData.h>


constexpr unsigned int NUM_PAYLOAD_VALUES = 4u;
constexpr unsigned int NUM_ATTRIBUTE_VALUES = 4u;
constexpr unsigned int MAX_TRACE_DEPTH = 8u;

enum RayType
{
    RAY_TYPE_RADIANCE  = 0,
    RAY_TYPE_OCCLUSION = 1,
    RAY_TYPE_COUNT = 2
};



struct HitGroupData
{
    sutil::Sphere       sphere;
    sutil::MaterialData material_data;

    unsigned int dc_index;
};

struct EmptyData {};


struct LaunchParams
{
    unsigned int                    width;
    unsigned int                    height;
    unsigned int                    subframe_index;
    float4*                         accum_buffer;
    uchar4*                         frame_buffer;
    int                             max_depth;
    float                           scene_epsilon;

    float3                          eye;
    float3                          U;
    float3                          V;
    float3                          W;

    sutil::BufferView<sutil::Light> lights;
    float3                          miss_color;
    OptixTraversableHandle          handle;
};
