/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "OptiXDenoiser.h"

struct ApplyFlow
{
    ApplyFlow() {}
    ~ApplyFlow() {}

    bool init( OptixImage2D color, CUstream stream ) {
        m_prevOut = createOptixImage2D( color.width, color.height, OPTIX_PIXEL_FORMAT_FLOAT4 );
        return cudaMemcpyAsync( (void*)m_prevOut.data, (void*)color.data,
                                color.rowStrideInBytes * color.height, cudaMemcpyDeviceToDevice, stream ) == cudaSuccess;
    }
    bool exit() { return freeOptixImage2D( m_prevOut ); }

    bool apply( OptixImage2D& out, const OptixImage2D& color, const OptixImage2D& flow, float flowMulX, float flowMulY, CUstream stream );

private:
    float catmull_rom(
        float       p[4],
        float       t);

    void warp(
        float4*             result,
        const float4*       image,
        const float4*       flow,
        unsigned int        x,
        unsigned int        y,
        float               flowMulX,
        float               flowMulY );

    OptixImage2D        m_prevOut = {};
};

inline float ApplyFlow::catmull_rom(
    float       p[4],
    float       t)
{
    return p[1] + 0.5f * t * ( p[2] - p[0] + t * ( 2.f * p[0] - 5.f * p[1] + 4.f * p[2] - p[3] + t * ( 3.f * ( p[1] - p[2]) + p[3] - p[0] ) ) );
}

// Apply flow to image at given pixel position (using bilinear interpolation), write back RGB result.
void ApplyFlow::warp(
    float4*             result,
    const float4*       image,
    const float4*       flow,
    unsigned int        x,
    unsigned int        y,
    float               flowMulX,
    float               flowMulY )
{
    unsigned int width  = m_prevOut.width;
    unsigned int height = m_prevOut.height;

    float dst_x = float( x ) - flow[x + y * width].x * flowMulX;
    float dst_y = float( y ) - flow[x + y * width].y * flowMulY;

    float x0 = dst_x - 1.f;
    float y0 = dst_y - 1.f;

    float r[4][4], g[4][4], b[4][4];
    for (int j=0; j < 4; j++)
    {
        for (int k=0; k < 4; k++)
        {
            int tx = static_cast<int>( x0 ) + k;
            if( tx < 0 )
                tx = 0;
            else if( tx >= (int)width )
                tx = width - 1;

            int ty = static_cast<int>( y0 ) + j;
            if( ty < 0 )
                ty = 0;
            else if( ty >= (int)height )
                ty = height - 1;

            r[j][k] = image[tx + ty * width].x;
            g[j][k] = image[tx + ty * width].y;
            b[j][k] = image[tx + ty * width].z;
        }
    }
    float tx = dst_x <= 0.f ? 0.f : dst_x - floorf( dst_x );

    r[0][0] = catmull_rom( r[0], tx );
    r[0][1] = catmull_rom( r[1], tx );
    r[0][2] = catmull_rom( r[2], tx );
    r[0][3] = catmull_rom( r[3], tx );

    g[0][0] = catmull_rom( g[0], tx );
    g[0][1] = catmull_rom( g[1], tx );
    g[0][2] = catmull_rom( g[2], tx );
    g[0][3] = catmull_rom( g[3], tx );

    b[0][0] = catmull_rom( b[0], tx );
    b[0][1] = catmull_rom( b[1], tx );
    b[0][2] = catmull_rom( b[2], tx );
    b[0][3] = catmull_rom( b[3], tx );

    float ty = dst_y <= 0.f ? 0.f : dst_y - floorf( dst_y );

    result[y * width + x].x = catmull_rom( r[0], ty );
    result[y * width + x].y = catmull_rom( g[0], ty );
    result[y * width + x].z = catmull_rom( b[0], ty );
}

// Apply flow from current frame to the previous noisy image, write result to out (must be allocated)
bool ApplyFlow::apply( OptixImage2D& out, const OptixImage2D& color, const OptixImage2D& flow,
                       float flowMulX, float flowMulY, CUstream stream )
{
    if( flow.data == 0 || flow.format != OPTIX_PIXEL_FORMAT_FLOAT4 ||
        out.width != flow.width || out.height != flow.height || out.format != OPTIX_PIXEL_FORMAT_FLOAT4 ||
        m_prevOut.width != flow.width || m_prevOut.height != flow.height || m_prevOut.format != OPTIX_PIXEL_FORMAT_FLOAT4 )
        return false;

    size_t image_size = flow.width * flow.height;

    bool ret = true;

    float4* flowdata = new float4[ image_size ]; 
    if( cudaMemcpyAsync( flowdata, (void*)flow.data, sizeof( float ) * 4 * image_size, cudaMemcpyDeviceToHost, stream ) != cudaSuccess )
        ret = false;

    float4* image = new float4[ image_size ];
    float4* result = new float4[ image_size ];

    if( cudaMemcpyAsync( image, (float4*)m_prevOut.data, sizeof( float ) * 4 * image_size, cudaMemcpyDeviceToHost, stream ) != cudaSuccess )
        ret = false;

    cudaStreamSynchronize( stream );

    for( unsigned int y=0; y < flow.height; y++ )
        for( unsigned int x=0; x < flow.width; x++ )
            warp( result, image, flowdata, x, y, flowMulX, flowMulY );

    if( cudaMemcpyAsync( (void*)out.data, result, sizeof( float ) * 4 * image_size, cudaMemcpyHostToDevice, stream ) != cudaSuccess )
        ret = false;

    delete[] result;
    delete[] image;
    delete[] flowdata;

    if( cudaMemcpyAsync( (void*)m_prevOut.data, (void*)color.data,
                         color.rowStrideInBytes * color.height, cudaMemcpyDeviceToDevice, stream ) != cudaSuccess )
        ret = false;

    return ret;
}
