/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "NtcTextureSet.h"
#include "NtcInference.h"


static __forceinline__ __device__ int CLAMP( int x, int a, int b )
{
    return ( x <= a ) ? a : ( x >= b ) ? b : x;
}

template <class T_VEC_OUT>
__device__ __forceinline__ 
static bool inferTexel( T_VEC_OUT& out, NtcTextureSetConstants& tsc, uint8_t* latents, uint8_t* mlpWeights, int x, int y, int lod )
{
    if (params.bound.enableFP8) {
        switch (params.bound.networkVersion) {
            case NTC_NETWORK_SMALL:  return inferTexelCoopVec_fp8<T_VEC_OUT, NTC_NETWORK_SMALL> ( out, tsc, latents, mlpWeights, x, y, lod ); break;
            case NTC_NETWORK_MEDIUM: return inferTexelCoopVec_fp8<T_VEC_OUT, NTC_NETWORK_MEDIUM>( out, tsc, latents, mlpWeights, x, y, lod ); break;
            case NTC_NETWORK_LARGE:  return inferTexelCoopVec_fp8<T_VEC_OUT, NTC_NETWORK_LARGE> ( out, tsc, latents, mlpWeights, x, y, lod ); break;
            case NTC_NETWORK_XLARGE: return inferTexelCoopVec_fp8<T_VEC_OUT, NTC_NETWORK_XLARGE>( out, tsc, latents, mlpWeights, x, y, lod ); break;
        }
    } else {
        switch (params.bound.networkVersion) {
            case NTC_NETWORK_SMALL:  return inferTexelCoopVec_int8<T_VEC_OUT, NTC_NETWORK_SMALL> ( out, tsc, latents, mlpWeights, x, y, lod ); break;
            case NTC_NETWORK_MEDIUM: return inferTexelCoopVec_int8<T_VEC_OUT, NTC_NETWORK_MEDIUM>( out, tsc, latents, mlpWeights, x, y, lod ); break;
            case NTC_NETWORK_LARGE:  return inferTexelCoopVec_int8<T_VEC_OUT, NTC_NETWORK_LARGE> ( out, tsc, latents, mlpWeights, x, y, lod ); break;
            case NTC_NETWORK_XLARGE: return inferTexelCoopVec_int8<T_VEC_OUT, NTC_NETWORK_XLARGE>( out, tsc, latents, mlpWeights, x, y, lod ); break;
        }
    }
    return false;
}

template <class T_VEC_OUT>
__forceinline__ __device__ 
static bool ntcTex2D( T_VEC_OUT& out, NtcTextureSet* ntsPtr, float u, float v, float2 xi )
{
    const float2 texelJitter = xi - float2{ 0.5f, 0.5f };  // stochastic bilinear sampling

    const NtcTextureSet& nts = *ntsPtr;

    const int i = CLAMP( u * nts.constants.imageWidth + texelJitter.x, 0, nts.constants.imageWidth - 1 );
    const int j = CLAMP( v * nts.constants.imageHeight + texelJitter.y, 0, nts.constants.imageHeight - 1 );

    const int mipLevel = 0;

    return inferTexel( out, ntsPtr->constants, nts.d_latents, nts.d_mlpWeights, i, j, mipLevel );
}
