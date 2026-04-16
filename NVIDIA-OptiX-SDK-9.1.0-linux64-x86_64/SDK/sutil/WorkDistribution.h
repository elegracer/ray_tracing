/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/Preprocessor.h>

#include <stdint.h>

class StaticWorkDistribution
{
public:
    SUTIL_INLINE SUTIL_HOSTDEVICE void setRasterSize( int width, int height )
    {
        m_width = width;
        m_height = height;
    }


    SUTIL_INLINE SUTIL_HOSTDEVICE void setNumGPUs( int32_t num_gpus )
    {
        m_num_gpus = num_gpus;
    }


    SUTIL_INLINE SUTIL_HOSTDEVICE int32_t numSamples( int32_t gpu_idx )
    {
        const int tile_strip_width  = TILE_WIDTH*m_num_gpus;
        const int tile_strip_height = TILE_HEIGHT;
        const int num_tile_strip_cols = m_width /tile_strip_width  + ( m_width %tile_strip_width  == 0 ? 0 : 1 );
        const int num_tile_strip_rows = m_height/tile_strip_height + ( m_height%tile_strip_height == 0 ? 0 : 1 );
        return num_tile_strip_rows*num_tile_strip_cols*TILE_WIDTH*TILE_HEIGHT;
    }


    SUTIL_INLINE SUTIL_HOSTDEVICE int2 getSamplePixel( int32_t gpu_idx, int32_t sample_idx )
    {
        const int tile_strip_width  = TILE_WIDTH*m_num_gpus;
        const int tile_strip_height = TILE_HEIGHT;
        const int num_tile_strip_cols = m_width /tile_strip_width + ( m_width % tile_strip_width == 0 ? 0 : 1 );

        const int tile_strip_idx     = sample_idx / (TILE_WIDTH*TILE_HEIGHT );
        const int tile_strip_y       = tile_strip_idx / num_tile_strip_cols;
        const int tile_strip_x       = tile_strip_idx - tile_strip_y * num_tile_strip_cols;
        const int tile_strip_x_start = tile_strip_x * tile_strip_width;
        const int tile_strip_y_start = tile_strip_y * tile_strip_height;

        const int tile_pixel_idx     = sample_idx - ( tile_strip_idx * TILE_WIDTH*TILE_HEIGHT );
        const int tile_pixel_y       = tile_pixel_idx / TILE_WIDTH;
        const int tile_pixel_x       = tile_pixel_idx - tile_pixel_y * TILE_WIDTH;

        const int tile_offset_x      = ( gpu_idx + tile_strip_y % m_num_gpus ) % m_num_gpus * TILE_WIDTH;

        const int pixel_y = tile_strip_y_start + tile_pixel_y;
        const int pixel_x = tile_strip_x_start + tile_pixel_x + tile_offset_x ;
        return make_int2( pixel_x, pixel_y );
    }


private:
    int32_t m_num_gpus = 0;
    int32_t m_width    = 0;
    int32_t m_height   = 0;

    static const int32_t TILE_WIDTH  = 8;
    static const int32_t TILE_HEIGHT = 4;
};
