/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/cuda/GeometryData.h>
#include <sutil/cuda/MaterialData.h>

enum RayType
{
    RAY_TYPE_RADIANCE = 0,
    RAY_TYPE_COUNT
};

struct Ray;
struct Hit;

struct HitGroupData
{
    sutil::TriangleMesh triangle_data;
    sutil::MaterialData material_data;
};

struct Params
{
    OptixTraversableHandle handle;
    Ray*                   rays;
    Hit*                   hits;
};

