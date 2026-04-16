/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/cuda/LocalGeometry.h>
#include <sutil/cuda/MaterialData.h>

namespace sutil
{

//------------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------------

template<typename T>
__device__ __forceinline__ T sampleTexture( MaterialData::Texture tex, const LocalGeometry &geom )
{
    if( tex.tex )
    {
        const float2 UV = geom.texcoord[tex.texcoord].UV * tex.texcoord_scale;
        const float2 rotation = tex.texcoord_rotation;
        const float2 UV_trans = make_float2(
            dot( UV, make_float2( rotation.y, rotation.x ) ),
            dot( UV, make_float2( -rotation.x, rotation.y ) ) ) + tex.texcoord_offset;
        return tex2D<float4>( tex.tex, UV_trans.x, UV_trans.y );
    }
    else
    {
        return T();
    }
}

} // namespace sutil
