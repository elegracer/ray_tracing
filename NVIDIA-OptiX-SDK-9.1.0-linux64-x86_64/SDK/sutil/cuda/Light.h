/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vector_types.h>

#include <sutil/Preprocessor.h>

namespace sutil
{

struct Light
{
    Light() {}

    enum class Falloff : int
    {
        NONE = 0,
        LINEAR,
        QUADRATIC
    };


    enum class Type : int
    {
        POINT   = 0,
        AMBIENT = 1
    };

    struct Point
    {
        float3   color      CONST_STATIC_INIT( { 1.0f, 1.0f, 1.0f } );
        float    intensity  CONST_STATIC_INIT( 1.0f                 );
        float3   position   CONST_STATIC_INIT( {}                   );
        Falloff  falloff    CONST_STATIC_INIT( Falloff::QUADRATIC   );
    };


    struct Ambient
    {
        float3   color      CONST_STATIC_INIT( {1.0f, 1.0f, 1.0f} );
    };


    Type  type;

    union
    {
        Point   point;
        Ambient ambient;
    };
};

} // namespace sutil
