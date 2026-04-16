/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/cuda/BufferView.h>
#include <sutil/vec_math.h>

#ifndef __CUDACC_RTC__
#    include <cassert>
#else
#    define assert(x) /*nop*/
#endif

namespace sutil
{

// unaligned equivalent of float2
struct Vec2f
{
    SUTIL_HOSTDEVICE operator float2() const { return { x, y }; }

    float x, y;
};

struct Vec4f
{
    SUTIL_HOSTDEVICE operator float4() const { return { x, y, z, w }; }

    float x, y, z, w;
};

//------------------------------------------------------------------------------
//
// Basic geometry types
//
//------------------------------------------------------------------------------

struct TriangleMesh
{
    // The number of supported texture spaces per mesh.
    static constexpr unsigned int num_texcoords = 2;
    GenericBufferView  indices;
    BufferView<float3> positions;
    BufferView<float3> normals;
    BufferView<Vec2f>  texcoords[num_texcoords]; // The buffer view may not be aligned, so don't use float2
    BufferView<Vec4f>  colors;                    // The buffer view may not be aligned, so don't use float4
};


struct Sphere
{
    float3 center;
    float  radius;
};


struct SphereShell
{
    enum HitType
    {
        HIT_OUTSIDE_FROM_OUTSIDE = 1u << 0,
        HIT_OUTSIDE_FROM_INSIDE  = 1u << 1,
        HIT_INSIDE_FROM_OUTSIDE  = 1u << 2,
        HIT_INSIDE_FROM_INSIDE   = 1u << 3
    };

    float3 center;
    float  radius1;
    float  radius2;
};


struct Parallelogram
{
    Parallelogram() = default;
    Parallelogram( float3 v1, float3 v2, float3 anchor )
        : v1( v1 )
        , v2( v2 )
        , anchor( anchor )
    {
        float3 normal = normalize( cross( v1, v2 ) );
        float  d      = dot( normal, anchor );
        this->v1 *= 1.0f / dot( v1, v1 );
        this->v2 *= 1.0f / dot( v2, v2 );
        plane = make_float4( normal, d );
    }
    float4 plane;
    float3 v1;
    float3 v2;
    float3 anchor;
};


struct Curves
{
    BufferView<float2> strand_u;     // strand_u at segment start per segment
    GenericBufferView  strand_i;     // strand index per segment
    BufferView<uint2>  strand_info;  // info.x = segment base
                                     // info.y = strand length (segments)
};


//------------------------------------------------------------------------------
//
// Generic container for the basic geometry types
//
//------------------------------------------------------------------------------

struct GeometryData
{
    enum Type
    {
        TRIANGLE_MESH         = 0,
        SPHERE                = 1,
        SPHERE_SHELL          = 2,
        PARALLELOGRAM         = 3,
        LINEAR_CURVE_ARRAY    = 4,
        QUADRATIC_CURVE_ARRAY = 5,
        CUBIC_CURVE_ARRAY     = 6,
        CATROM_CURVE_ARRAY    = 7,
        UNKNOWN_TYPE          = 8
    };

    GeometryData() {};

    void setTriangleMesh( const TriangleMesh& t )
    {
        assert( type == UNKNOWN_TYPE );
        type          = TRIANGLE_MESH;
        triangle_mesh = t;
    }

    SUTIL_HOSTDEVICE const TriangleMesh& getTriangleMesh() const
    {
        assert( type == TRIANGLE_MESH );
        return triangle_mesh;
    }

    void setSphere( const Sphere& s )
    {
        assert( type == UNKNOWN_TYPE );
        type   = SPHERE;
        sphere = s;
    }

    SUTIL_HOSTDEVICE const Sphere& getSphere() const
    {
        assert( type == SPHERE );
        return sphere;
    }

    void setSphereShell( const SphereShell& s )
    {
        assert( type == UNKNOWN_TYPE );
        type         = SPHERE_SHELL;
        sphere_shell = s;
    }

    SUTIL_HOSTDEVICE const SphereShell& getSphereShell() const
    {
        assert( type == SPHERE_SHELL );
        return sphere_shell;
    }

    void setParallelogram( const Parallelogram& p )
    {
        assert( type == UNKNOWN_TYPE );
        type          = PARALLELOGRAM;
        parallelogram = p;
    }

    SUTIL_HOSTDEVICE const Parallelogram& getParallelogram() const
    {
        assert( type == PARALLELOGRAM );
        return parallelogram;
    }

    SUTIL_HOSTDEVICE const Curves& getCurveArray() const
    {
        assert( type == LINEAR_CURVE_ARRAY || type == QUADRATIC_CURVE_ARRAY || type == CUBIC_CURVE_ARRAY || type == CATROM_CURVE_ARRAY );
        return curves;
    }


    void setLinearCurveArray( const Curves& c )
    {
        assert( type == UNKNOWN_TYPE );
        type   = LINEAR_CURVE_ARRAY;
        curves = c;
    }

    SUTIL_HOSTDEVICE const Curves& getLinearCurveArray() const
    {
        assert( type == LINEAR_CURVE_ARRAY );
        return curves;
    }

    void setQuadraticCurveArray( const Curves& c )
    {
        assert( type == UNKNOWN_TYPE );
        type   = QUADRATIC_CURVE_ARRAY;
        curves = c;
    }

    SUTIL_HOSTDEVICE const Curves& getQuadraticCurveArray() const
    {
        assert( type == QUADRATIC_CURVE_ARRAY );
        return curves;
    }

    void setCubicCurveArray( const Curves& c )
    {
        assert( type == UNKNOWN_TYPE );
        type   = CUBIC_CURVE_ARRAY;
        curves = c;
    }

    SUTIL_HOSTDEVICE const Curves& getCubicCurveArray() const
    {
        assert( type == CUBIC_CURVE_ARRAY );
        return curves;
    }

    void setCatromCurveArray( const Curves& c )
    {
        assert( type == UNKNOWN_TYPE );
        type   = CATROM_CURVE_ARRAY;
        curves = c;
    }

    SUTIL_HOSTDEVICE const Curves& getCatromCurveArray() const
    {
        assert( type == CATROM_CURVE_ARRAY );
        return curves;
    }

    Type  type = UNKNOWN_TYPE;

  private:
    union
    {
        TriangleMesh  triangle_mesh;
        Sphere        sphere;
        SphereShell   sphere_shell;
        Parallelogram parallelogram;
        Curves        curves;
    };
};

} // namespace sutil
