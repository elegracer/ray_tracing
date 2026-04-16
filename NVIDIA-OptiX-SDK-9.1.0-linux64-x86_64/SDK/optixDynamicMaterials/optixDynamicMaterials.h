/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

struct Params
{
    uchar4*                image;
    unsigned int           image_width;
    unsigned int           image_height;
    float                  radius;
    OptixTraversableHandle handle;
    float3                 cam_eye;
    float3                 camera_u, camera_v, camera_w;
    unsigned int           hitgroupRecordIdx_0;
    unsigned int           hitgroupRecordStride;
};


struct RayGenData
{
    float3 cam_eye;
    float3 camera_u, camera_v, camera_w;
};


struct MissData
{
    float3 color;
};


struct HitGroupData
{
    float3       color;
    unsigned int geometryIndex;
};
