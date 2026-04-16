/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

// includes CUDA Runtime
#include <cuda_runtime.h>
#include <sutil/Exception.h>

#include <vector>


template <typename T>
void copyToDevice( const T& source, CUdeviceptr destination )
{
    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( destination ), &source, sizeof( T ), cudaMemcpyHostToDevice ) );
}

template <typename T>
void createOnDevice( const T& source, CUdeviceptr* destination )
{
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( destination ), sizeof( T ) ) );
    copyToDevice( source, *destination );
}

template <typename T>
void copyToDevice( const std::vector<T>& source, CUdeviceptr destination )
{
    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( destination ), source.data(), source.size() * sizeof( T ), cudaMemcpyHostToDevice ) );
}

template <typename T>
void createOnDevice( const std::vector<T>& source, CUdeviceptr* destination )
{
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( destination ), source.size() * sizeof( T ) ) );
    copyToDevice( source, *destination );
}


inline std::ostream& operator<<( std::ostream& o, float3 v )
{
    o << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return o;
}
