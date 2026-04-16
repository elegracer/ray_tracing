/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vector_types.h>

#include <sutil/cuda/BufferView.h>
#include <sutil/cuda/GeometryData.h>
#include <sutil/cuda/Light.h>
#include <sutil/cuda/MaterialData.h>

#include "NtcTextureSet.h"
#include <libntc/shaders/InferenceConstants.h>


const unsigned int NUM_ATTRIBUTE_VALUES = 4u;
const unsigned int NUM_PAYLOAD_VALUES   = 3u;
const unsigned int MAX_TRACE_DEPTH      = 8u;


struct HitGroupData
{
    sutil::GeometryData geometry_data;
    sutil::MaterialData material_data;
};


enum RayType
{
    RAY_TYPE_RADIANCE  = 0,
    RAY_TYPE_OCCLUSION = 1,
    RAY_TYPE_COUNT     = 2
};


struct BoundValues
{
    float3 miss_color;
    int    networkVersion;
    bool   enableFP8;

    uint32_t samples_per_launch;
};


struct LaunchParams
{
    unsigned int width;
    unsigned int height;
    unsigned int subframe_index;
    float4*      accum_buffer;
    uchar4*      frame_buffer;
    int          max_depth;
    float        scene_epsilon;

    float3 eye;
    float3 U;
    float3 V;
    float3 W;

    sutil::BufferView<sutil::Light> lights;
    OptixTraversableHandle          handle;

    // constants we can specialize for
    BoundValues bound;

    // The texture set for the mesh
    NtcTextureSet* textureSet;
};


struct PayloadRadiance
{
    float3 result;
    float  importance;
};


struct PayloadOcclusion
{
    float3 result;
};
