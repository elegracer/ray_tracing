/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <optix.h>


enum AccelBuildMode
{
    CLUSTER, 
    FLAT_TRIANGLE,
    ACCEL_BUILD_MODE_COUNT
};

struct Accel
{
public:
    virtual ~Accel() = default;
};

struct FlatTriAccel : public Accel
{
    CUdeviceptr            d_gasBuffer   = 0;
    size_t                 gasBufferSize = 0;
    OptixTraversableHandle gasHandle     = 0;
};

struct ClusterAccel : public Accel
{
    CUdeviceptr             d_gasBuffer = 0;
    size_t                  gasBufferSize = 0;
    CUdeviceptr*            d_gasPtrsBuffer = nullptr;  // handles in device memory
    unsigned int*           d_gasSizesBuffer = nullptr;

    CUdeviceptr*            d_clasPtrsBuffer = nullptr;  // address of each CLAS in the CLAS buffer
    CUdeviceptr             d_clasBuffer     = 0;        // CLAS buffer
};

struct Params
{
    uchar4*                frameBuffer;
    unsigned int           width;
    unsigned int           height;
    float3                 eye, U, V, W;
    OptixTraversableHandle handle;
    int                    subframeIndex;
    AccelBuildMode         accelBuildMode;
    uint2                  gridDims;       // for shading; 
};

struct RayGenData
{
};


struct MissData
{
    float4 bg_color;
};


struct HitGroupData
{
};