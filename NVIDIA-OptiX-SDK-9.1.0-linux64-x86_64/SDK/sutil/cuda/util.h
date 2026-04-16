/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#ifndef __CUDACC_RTC__
#    include <stdio.h>
#endif

#define if_pixel( x_, y_ )                                                     \
    const uint3 launch_idx__ = optixGetLaunchIndex();                          \
    if( launch_idx__.x == (x_) && launch_idx__.y == (y_) )                     \

#define print_pixel( x_, y_, str, ... )                                        \
do                                                                             \
{                                                                              \
    const uint3 launch_idx = optixGetLaunchIndex();                            \
    if( launch_idx.x == (x_) && launch_idx.y == (y_) )                         \
    {                                                                          \
         printf( str, __VA_ARGS__  );                                          \
    }                                                                          \
} while(0);


