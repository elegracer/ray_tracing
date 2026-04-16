/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

enum RayType
{
    RAY_TYPE_RADIANCE  = 0,
    RAY_TYPE_OCCLUSION = 1,
    RAY_TYPE_COUNT
};


struct ParallelogramLight
{
    float3 corner;
    float3 v1, v2;
    float3 normal;
    float3 emission;
};


struct Params
{
    unsigned int subframe_index;
    float4*      accum_buffer;
    uchar4*      frame_buffer;
    unsigned int width;
    unsigned int height;
    unsigned int samples_per_launch;
    unsigned int light_samples;

    float3       eye;
    float3       U;
    float3       V;
    float3       W;

    ParallelogramLight     light; // TODO: make light list
    OptixTraversableHandle handle;
};


struct RayGenData
{
};


struct MissData
{
    float4 bg_color;
};


struct HitGroupData
{
    float3  emission_color;
    float3  diffuse_color;
    float4* vertices;
};

#if defined( __CUDACC__ )

#include <sutil/cuda/helpers.h>
#include <sutil/vec_math.h>

struct RadiancePRD
{
    // TODO: move some state directly into payload registers?
    float3       emitted;
    float3       radiance;
    float3       attenuation;
    float3       origin;
    float3       direction;
    unsigned int seed;
    int          countEmitted;
    int          done;
    int          pad;
};


struct Onb
{
    __forceinline__ __device__ Onb( const float3& normal )
    {
        m_normal = normal;

        if( fabs( m_normal.x ) > fabs( m_normal.z ) )
        {
            m_binormal.x = -m_normal.y;
            m_binormal.y =  m_normal.x;
            m_binormal.z =  0;
        }
        else
        {
            m_binormal.x =  0;
            m_binormal.y = -m_normal.z;
            m_binormal.z =  m_normal.y;
        }

        m_binormal = normalize( m_binormal );
        m_tangent = cross( m_binormal, m_normal );
    }

    __forceinline__ __device__ void inverse_transform( float3& p ) const
    {
        p = p.x*m_tangent + p.y*m_binormal + p.z*m_normal;
    }

    float3 m_tangent;
    float3 m_binormal;
    float3 m_normal;
};


static __forceinline__ __device__ void* unpackPointer( unsigned int i0, unsigned int i1 )
{
    const unsigned long long uptr = static_cast<unsigned long long>(i0) << 32 | i1;
    void*           ptr = reinterpret_cast<void*>(uptr);
    return ptr;
}


static __forceinline__ __device__ void  packPointer( void* ptr, unsigned int& i0, unsigned int& i1 )
{
    const unsigned long long uptr = reinterpret_cast<unsigned long long>(ptr);
    i0 = uptr >> 32;
    i1 = uptr & 0x00000000ffffffff;
}

static __forceinline__ __device__ RadiancePRD* getPRD()
{
    const unsigned int u0 = optixGetPayload_0();
    const unsigned int u1 = optixGetPayload_1();
    return reinterpret_cast<RadiancePRD*>(unpackPointer( u0, u1 ));
}

#endif
