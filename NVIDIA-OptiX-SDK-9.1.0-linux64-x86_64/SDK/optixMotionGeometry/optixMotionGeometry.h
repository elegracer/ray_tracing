/*
 * SPDX-FileCopyrightText: Copyright (c) 2021 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

struct Params
{
    uchar4*                frame_buffer;
    unsigned int           width;
    unsigned int           height;
    unsigned int           spp;
    float3                 eye, U, V, W;
    OptixTraversableHandle handle;
    int                    subframe_index;
    bool                   ao;
};

struct RayGenData
{
    float3 cam_eye;
    float3 camera_u, camera_v, camera_w;
};


struct MissData
{
    float4 bg_color;
};


struct HitGroupData
{
    float3 color;
};
