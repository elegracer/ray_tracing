/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/Aabb.h>
#include <sutil/CUDAOutputBuffer.h>
#include <sutil/Record.h>
#include <sutil/Camera.h>
#include <sutil/cuda/BufferView.h>

#include <cuda_runtime.h>

#include "optixHair.cuh"

typedef sutil::EmptyRecord           RayGenRecord;
typedef sutil::EmptyRecord           MissRecord;
typedef sutil::Record<HitGroupData>  HitRecord;

//
// forward declarations
//

class Hair;
class Head;
class Camera;
class ShaderBindingTable;
class HairProgramGroups;

struct HairState
{
    unsigned int buildFlags = OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_ALLOW_RANDOM_VERTEX_ACCESS | OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;

    OptixDeviceContext context = 0;

    Hair*         pHair;
    const Head*   pHead;
    sutil::Camera camera = {};

    unsigned int width  = 0;
    unsigned int height = 0;

    sutil::CUDAOutputBuffer<uchar4> outputBuffer = sutil::CUDAOutputBuffer<uchar4>(sutil::CUDAOutputBufferType::CUDA_DEVICE, 1, 1);
    sutil::CUDAOutputBuffer<float4> accumBuffer = sutil::CUDAOutputBuffer<float4>(sutil::CUDAOutputBufferType::CUDA_DEVICE, 1, 1);

    sutil::Aabb aabb;

    LaunchParams  params       = {};
    LaunchParams* deviceParams = nullptr;

    sutil::Light lights[2] = {};

    OptixTraversableHandle hHairGAS            = 0;
    CUdeviceptr            deviceBufferHairGAS = 0;

    OptixTraversableHandle hIAS            = 0;
    CUdeviceptr            deviceBufferIAS = 0;

    // for curves SBT record
    sutil::Curves curves = {};

    //ShaderBindingTable* pSBT           = nullptr;
    HairProgramGroups*  pProgramGroups = nullptr;
    OptixPipeline       pipeline       = 0;
    OptixShaderBindingTable SBT = {};

};

void makeHairGAS( HairState* pState );
void makeInstanceAccelerationStructure( HairState* pState );
void makePipeline( HairState* pState );
void makeProgramGroups( HairState* pState );
void makeSBT( HairState* pState );
void renderFrame( HairState* pState );

void initializeParams( HairState* pState );
void updateParams( HairState* pState );
void updateSize( HairState* pState, int width, int height );
