/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>

#include "helpers.h"
#include "GeometryData.h"


#define float3_as_uints( u ) __float_as_uint( u.x ), __float_as_uint( u.y ), __float_as_uint( u.z )


namespace sutil
{

__device__ __forceinline__ void intersectSphere(
    float3 center,
    float  radius
)
{
    const float3 ray_orig = optixGetWorldRayOrigin();
    const float3 ray_dir  = optixGetWorldRayDirection();
    const float  ray_tmin = optixGetRayTmin();
    const float  ray_tmax = optixGetRayTmax();

    const float3 O      = ray_orig - center;
    const float  l      = 1.0f / length( ray_dir );
    const float3 D      = ray_dir * l;

    float b    = dot( O, D );
    float c    = dot( O, O ) - radius * radius;
    float disc = b * b - c;
    if( disc > 0.0f )
    {
        float sdisc        = sqrtf( disc );
        float root1        = ( -b - sdisc );
        float root11       = 0.0f;
        bool  check_second = true;

        const bool do_refine = fabsf( root1 ) > ( 10.0f * radius );

        if( do_refine )
        {
            // refine root1
            float3 O1 = O + root1 * D;
            b         = dot( O1, D );
            c         = dot( O1, O1 ) - radius * radius;
            disc      = b * b - c;

            if( disc > 0.0f )
            {
                sdisc  = sqrtf( disc );
                root11 = ( -b - sdisc );
            }
        }

        float  t;
        float3 normal;
        t = ( root1 + root11 ) * l;
        if( t > ray_tmin && t < ray_tmax )
        {
            normal = ( O + ( root1 + root11 ) * D ) / radius;
            if( optixReportIntersection( t, 0, float3_as_uints( normal ), __float_as_uint( radius ) ) )
                check_second = false;
        }

        if( check_second )
        {
            float root2 = ( -b + sdisc ) + ( do_refine ? root1 : 0 );
            t           = root2 * l;
            normal      = ( O + root2 * D ) / radius;
            if( t > ray_tmin && t < ray_tmax )
                optixReportIntersection( t, 0, float3_as_uints( normal ), __float_as_uint( radius ) );
        }
    }
}


__device__ __forceinline__ void intersectParallelogram(
    const float4 plane,
    const float3 anchor,
    const float3 v1,
    const float3 v2
)
{
    const float3 ray_orig = optixGetWorldRayOrigin();
    const float3 ray_dir  = optixGetWorldRayDirection();
    const float  ray_tmin = optixGetRayTmin(), ray_tmax = optixGetRayTmax();

    float3 n  = make_float3( plane );
    float  dt = dot( ray_dir, n );
    float  t  = ( plane.w - dot( n, ray_orig ) ) / dt;
    if( t > ray_tmin && t < ray_tmax )
    {
        float3 p  = ray_orig + ray_dir * t;
        float3 vi = p - anchor;
        float  a1 = dot( v1, vi );
        if( a1 >= 0 && a1 <= 1 )
        {
            float a2 = dot( v2, vi );
            if( a2 >= 0 && a2 <= 1 )
            {
                optixReportIntersection( t, 0, float3_as_args( n ), __float_as_uint( a1 ), __float_as_uint( a2 ) );
            }
        }
    }
}


__device__ __forceinline__ void intersectSphereShell(
    const float3 center,
    const float  radius1,
    const float  radius2
)
{

    const float3 ray_orig = optixGetWorldRayOrigin();
    const float3 ray_dir  = optixGetWorldRayDirection();
    const float  ray_tmin = optixGetRayTmin(), ray_tmax = optixGetRayTmax();

    float3 O = ray_orig - center;
    float  l = 1 / length( ray_dir );
    float3 D = ray_dir * l;

    float b = dot( O, D ), sqr_b = b * b;
    float O_dot_O = dot( O, O );
    float sqr_radius1 = radius1 * radius1, sqr_radius2 = radius2 * radius2;

    // check if we are outside of outer sphere
    constexpr float EPSILON = 1e-4f;
    if( O_dot_O > sqr_radius2 + EPSILON )
    {
        if( O_dot_O - sqr_b < sqr_radius2 - EPSILON )
        {
            float c    = O_dot_O - sqr_radius2;
            float root = sqr_b - c;
            if( root > 0.0f )
            {
                float  t      = -b - sqrtf( root );
                float3 normal = ( O + t * D ) / radius2;
                optixReportIntersection( t * l, sutil::SphereShell::HIT_OUTSIDE_FROM_OUTSIDE, float3_as_args( normal ) );
            }
        }
    }
    // else we are inside of the outer sphere
    else
    {
        float c    = O_dot_O - sqr_radius1;
        float root = b * b - c;
        if( root > 0.0f )
        {
            float t = -b - sqrtf( root );
            // do we hit inner sphere from between spheres?
            if( t * l > ray_tmin && t * l < ray_tmax )
            {
                float3 normal = ( O + t * D ) / ( -radius1 );
                optixReportIntersection( t * l, sutil::SphereShell::HIT_INSIDE_FROM_OUTSIDE, float3_as_args( normal ) );
            }
            else
            {
                // do we hit inner sphere from within both spheres?
                t = -b + ( root > 0 ? sqrtf( root ) : 0.f );
                if( t * l > ray_tmin && t * l < ray_tmax )
                {
                    float3 normal = ( O + t * D ) / ( -radius1 );
                    optixReportIntersection( t * l, sutil::SphereShell::HIT_INSIDE_FROM_INSIDE, float3_as_args( normal ) );
                }
                else
                {
                    // do we hit outer sphere from between spheres?
                    c             = O_dot_O - sqr_radius2;
                    root          = b * b - c;
                    t             = -b + ( root > 0 ? sqrtf( root ) : 0.f );
                    float3 normal = ( O + t * D ) / radius2;
                    optixReportIntersection( t * l, sutil::SphereShell::HIT_OUTSIDE_FROM_INSIDE, float3_as_args( normal ) );
                }
            }
        }
        else
        {
            // do we hit outer sphere from between spheres?
            c             = O_dot_O - sqr_radius2;
            root          = b * b - c;
            float  t      = -b + ( root > 0 ? sqrtf( root ) : 0.f );
            float3 normal = ( O + t * D ) / radius2;
            optixReportIntersection( t * l, sutil::SphereShell::HIT_OUTSIDE_FROM_INSIDE, float3_as_args( normal ) );
        }
    }
}

} // namespace sutil

#undef float3_as_uints
