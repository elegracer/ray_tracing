/*
 * SPDX-FileCopyrightText: Copyright (c) 2021 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>
#include "optix_denoiser_opticalflow.h"


static inline unsigned int divUp( unsigned int nominator, unsigned int denominator )
{
    return ( nominator + denominator - 1 ) / denominator;
}

struct floatRdAccess
{
    inline floatRdAccess( const OptixImage2D& im )
        : image( im )
        , psb( im.pixelStrideInBytes )
        , hf( image.format == OPTIX_PIXEL_FORMAT_HALF2 || image.format == OPTIX_PIXEL_FORMAT_HALF3 || image.format == OPTIX_PIXEL_FORMAT_HALF4 )
    {
        if( im.pixelStrideInBytes == 0 )
        {
            unsigned int dsize = hf ? sizeof( __half ) : sizeof( float );
            psb                = getNumChannels( im ) * dsize;
        }
    }
    inline __device__ float read( int x, int y, int c ) const
    {
        if( hf )
            return float( *(const __half*)( image.data + y * image.rowStrideInBytes + x * psb + c * sizeof( __half ) ) );
        else
            return float( *(const float*)( image.data + y * image.rowStrideInBytes + x * psb + c * sizeof( float ) ) );
    }
    OptixImage2D image;
    unsigned int psb;
    bool         hf;
};

struct floatWrAccess
{
    inline floatWrAccess( const OptixImage2D& im )
        : image( im )
        , psb( im.pixelStrideInBytes )
        , hf( image.format == OPTIX_PIXEL_FORMAT_HALF2 || image.format == OPTIX_PIXEL_FORMAT_HALF3 || image.format == OPTIX_PIXEL_FORMAT_HALF4 )
    {
        if( im.pixelStrideInBytes == 0 )
        {
            unsigned int dsize = hf ? sizeof( __half ) : sizeof( float );
            psb                = getNumChannels( im ) * dsize;
        }
    }
    inline __device__ void write( int x, int y, int c, float value )
    {
        if( hf )
            *(__half*)( image.data + y * image.rowStrideInBytes + x * psb + c * sizeof( __half ) ) = value;
        else
            *(float*)( image.data + y * image.rowStrideInBytes + x * psb + c * sizeof( float ) ) = value;
    }
    OptixImage2D image;
    unsigned int psb;
    bool         hf;
};

static __global__ void k_convertRGBA( unsigned char* result, floatRdAccess input, int outStrideX )
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if( x >= input.image.width || y >= input.image.height )
        return;

    unsigned int r = __saturatef( input.read( x, y, 0 ) ) * 255.f;
    unsigned int g = __saturatef( input.read( x, y, 1 ) ) * 255.f;
    unsigned int b = __saturatef( input.read( x, y, 2 ) ) * 255.f;

    *(unsigned int*)&result[y * outStrideX + x * 4] = b | ( g << 8 ) | ( r << 16 ) | ( 255u << 24 );
}

static __global__ void k_convertFlow( floatWrAccess result, const int16_t* input, int inStrideX )
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if( x >= result.image.width || y >= result.image.height )
        return;

    result.write( x, y, 0, float( input[y * inStrideX + x * 2 + 0] ) * ( 1.f / 32.f ) );
    result.write( x, y, 1, float( input[y * inStrideX + x * 2 + 1] ) * ( 1.f / 32.f ) );
}

OptixResult convertRGBA( unsigned char* result, const OptixImage2D& input, uint32_t inStrideXInBytes, CUstream stream )
{
        dim3 block( 32, 32, 1 );
        dim3 grid = dim3( divUp( input.width, block.x ), divUp( input.height, block.y ), 1 );

        k_convertRGBA<<<grid, block, 0, stream>>>( result, floatRdAccess( input ), inStrideXInBytes );

        return OPTIX_SUCCESS;
}

OptixResult convertFlow( OptixImage2D& result, const int16_t* flow, uint32_t outStrideXInBytes, CUstream stream )
{
        dim3 block( 32, 32, 1 );
        dim3 grid = dim3( divUp( result.width, block.x ), divUp( result.height, block.y ), 1 );

        // convert 2x16 bit fixpoint to 2xfp16/2xfp32 bit flow vectors
        k_convertFlow<<<grid, block, 0, stream>>>( floatWrAccess( result ), flow, outStrideXInBytes );

        return OPTIX_SUCCESS;
}

extern OptixResult runOpticalFlow( CUcontext ctx, CUstream stream, OptixImage2D & flow, OptixImage2D input[2], float & flowTime, std::string & errMessage )
{
    OptixUtilOpticalFlow oflow;

    if( const OptixResult res = oflow.init( ctx, stream, input[0].width, input[0].height ) )
    {
        errMessage = oflow.getLastError();
        return res;
    }

    CUevent start, stop;
    cuEventCreate( &start, 0 );
    cuEventCreate( &stop, 0 );
    cuEventRecord( start, stream );
    
    if( const OptixResult res = oflow.computeFlow( flow, input ) )
    {
        errMessage = oflow.getLastError();
        cuEventDestroy( start );
        cuEventDestroy( stop );
        return res;
    }

    cuEventRecord(stop, stream);
    cuEventSynchronize( stop );
    cuEventElapsedTime(&flowTime, start, stop);

    cuEventDestroy( start );
    cuEventDestroy( stop );

    return oflow.destroy();
}
