/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>

#include <sutil/cuda/LocalGeometry.h>
#include <sutil/cuda/LocalShading.h>
#include <sutil/cuda/camera.h>
#include <sutil/cuda/geometry.h>
#include <sutil/cuda/helpers.h>
#include <sutil/cuda/microfacet.h>
#include <sutil/cuda/random.h>
#include <sutil/vec_math.h>

#include "optixWhitted.h"

extern "C" {
__constant__ LaunchParams params;
}


//------------------------------------------------------------------------------
//
// Local types
//
//------------------------------------------------------------------------------


struct PayloadRadiance
{
    float3 result;
    float  importance;
};


struct PayloadOcclusion
{
    float3 result;
};



//------------------------------------------------------------------------------
//
// Helper functions
//
//------------------------------------------------------------------------------

__device__ __inline__ PayloadRadiance getPayloadRadiance()
{
    PayloadRadiance prd;
    prd.result.x   = __uint_as_float( optixGetPayload_0() );
    prd.result.y   = __uint_as_float( optixGetPayload_1() );
    prd.result.z   = __uint_as_float( optixGetPayload_2() );
    prd.importance = __uint_as_float( optixGetPayload_3() );
    return prd;
}


__device__ __inline__ void setPayloadRadiance( const PayloadRadiance& prd )
{
    optixSetPayload_0( __float_as_uint( prd.result.x ) );
    optixSetPayload_1( __float_as_uint( prd.result.y ) );
    optixSetPayload_2( __float_as_uint( prd.result.z ) );
    optixSetPayload_3( __float_as_uint( prd.importance ) );
}


__device__ __inline__ PayloadOcclusion getPayloadOcclusion()
{
    PayloadOcclusion prd;
    prd.result.x = __uint_as_float( optixGetPayload_0() );
    prd.result.y = __uint_as_float( optixGetPayload_1() );
    prd.result.z = __uint_as_float( optixGetPayload_2() );
    return prd;
}


__device__ __inline__ void setPayloadOcclusion( const PayloadOcclusion& prd )
{
    optixSetPayload_0( __float_as_uint( prd.result.x ) );
    optixSetPayload_1( __float_as_uint( prd.result.y ) );
    optixSetPayload_2( __float_as_uint( prd.result.z ) );
}


__device__ __inline__ float3 payloadGetResult()
{
    return make_float3(
        __uint_as_float(optixGetPayload_0()),
        __uint_as_float(optixGetPayload_1()),
        __uint_as_float(optixGetPayload_2())
    );
}


__device__ __inline__ void payloadSetResult(const float3 result)
{
    optixSetPayload_0(__float_as_uint( result.x ));
    optixSetPayload_1(__float_as_uint( result.y ));
    optixSetPayload_2(__float_as_uint( result.z ));
}


__device__ __inline__ float payloadGetImportance()
{
    return __uint_as_float(optixGetPayload_3());
}

static __device__ __inline__ float3 traceRadianceRay( float3 origin, float3 direction, float importance )
{
    float3 result;
    float current_importance = importance;

    optixTrace(
        params.handle,
        origin,
        direction,
        params.scene_epsilon,
        1e16f,
        0.0f,
        OptixVisibilityMask(1),
        OPTIX_RAY_FLAG_NONE,
        RAY_TYPE_RADIANCE,
        RAY_TYPE_COUNT,
        RAY_TYPE_RADIANCE,
        float3_as_args(result),
        /* Can't use __float_as_uint() because it returns rvalue but payload requires a lvalue */
        reinterpret_cast<unsigned int&>( current_importance )
    );

    return result;
}


__device__ __forceinline__ void phongShadowed()
{
    // this material is opaque, so it fully attenuates all shadow rays
    const float3 occlusion_result = make_float3(0.0f);
    payloadSetResult(occlusion_result);
}

__device__ __forceinline__ void phongShade( float3 p_Kd, float3 p_Ka, float3 p_Ks, float3 p_Kr, float p_phong_exp, float3 p_normal )
{
    const float3 ray_orig = optixGetWorldRayOrigin();
    const float3 ray_dir  = optixGetWorldRayDirection();
    const float  ray_t    = optixGetRayTmax();


    float3 hit_point = ray_orig + ray_t * ray_dir;

    // ambient contribution
    sutil::Light::Ambient ambient_light = params.lights[0].ambient;
    float3         result        = p_Ka * ambient_light.color;

    // compute direct lighting
    sutil::Light::Point point_light = params.lights[1].point;
    float               Ldist       = length( point_light.position - hit_point );
    float3              L           = normalize( point_light.position - hit_point );
    float               nDl         = dot( p_normal, L );

    // cast shadow ray
    float3 light_attenuation = make_float3( static_cast<float>( nDl > 0.0f ) );
    if( nDl > 0.0f )
    {
        float3 occlusion_result = make_float3(1.0f);

        optixTrace(
            params.handle,
            hit_point,
            L,
            0.01f,
            Ldist,
            0.0f,
            OptixVisibilityMask( 1 ),
            OPTIX_RAY_FLAG_NONE,
            RAY_TYPE_OCCLUSION,
            RAY_TYPE_COUNT,
            RAY_TYPE_OCCLUSION,
            float3_as_args( occlusion_result )
        );

        light_attenuation = occlusion_result;
    }

    // If not completely shadowed, light the hit point
    if( fmaxf( light_attenuation ) > 0.0f )
    {
        float3 Lc = point_light.color * light_attenuation;

        result += p_Kd * nDl * Lc;

        float3 H   = normalize( L - ray_dir );
        float  nDh = dot( p_normal, H );
        if( nDh > 0 )
        {
            float power = pow( nDh, p_phong_exp );
            result += p_Ks * power * Lc;
        }
    }

    if( fmaxf( p_Kr ) > 0 )
    {
        // ray tree attenuation
        const float importance = payloadGetImportance();
        float new_importance = importance * sutil::luminance( p_Kr );

        // reflection ray
        if( new_importance >= 0.01f && optixGetRemainingTraceDepth() > 0 )
        {
            float3 R = reflect( ray_dir, p_normal );

            result += p_Kr * traceRadianceRay( hit_point, R, new_importance );
        }
    }

    // pass the color back
    payloadSetResult(result);
}


//------------------------------------------------------------------------------
//
// rendering programs
//
//------------------------------------------------------------------------------

extern "C" __global__ void __raygen__pinhole_camera()
{
    sutil::pinholeGenerateRay(
        params.subframe_index,
        params.accum_buffer,
        params.frame_buffer,
        params.eye,
        params.U,
        params.V,
        params.W,
        RAY_TYPE_RADIANCE,
        RAY_TYPE_COUNT,
        params.handle
    );
}


extern "C" __global__ void __miss__constant_bg()
{
    payloadSetResult(params.miss_color);
}


extern "C" __global__ void __intersection__sphere()
{
    const HitGroupData* sbt_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    const sutil::Sphere& sphere = sbt_data->geometry_data.getSphere();
    sutil::intersectSphere( sphere.center, sphere.radius );
}


extern "C" __global__ void __intersection__parallelogram()
{
    const HitGroupData*       sbt_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    const sutil::Parallelogram& pgram = sbt_data->geometry_data.getParallelogram();

    sutil::intersectParallelogram(
        pgram.plane,
        pgram.anchor,
        pgram.v1,
        pgram.v2
    );
}


extern "C" __global__ void __intersection__sphere_shell()
{
    const HitGroupData*     sbt_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    const sutil::SphereShell& sphere_shell = sbt_data->geometry_data.getSphereShell();
    sutil::intersectSphereShell(
        sphere_shell.center,
        sphere_shell.radius1,
        sphere_shell.radius2
    );
}


extern "C" __global__ void __closesthit__full_occlusion()
{
    phongShadowed();
}


//------------------------------------------------------------------------------
//
//  Checker shading
//
//------------------------------------------------------------------------------

extern "C" __global__ void __closesthit__checker_radiance()
{
    const HitGroupData* sbt_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer());
    const sutil::MaterialData::CheckerPhong& checker = sbt_data->material_data.checker;

    float3 Kd, Ka, Ks, Kr;
    float  phong_exp;

    float2 texcoord = make_float2( __uint_as_float( optixGetAttribute_3() ), __uint_as_float( optixGetAttribute_4() ) );
    float2 t        = texcoord * checker.inv_checker_size;
    t.x             = floorf( t.x );
    t.y             = floorf( t.y );

    int which_check = ( static_cast<int>( t.x ) + static_cast<int>( t.y ) ) & 1;

    if( which_check )
    {
        Kd        = checker.Kd1;
        Ka        = checker.Ka1;
        Ks        = checker.Ks1;
        Kr        = checker.Kr1;
        phong_exp = checker.phong_exp1;
    }
    else
    {
        Kd        = checker.Kd2;
        Ka        = checker.Ka2;
        Ks        = checker.Ks2;
        Kr        = checker.Kr2;
        phong_exp = checker.phong_exp2;
    }

    float3 object_normal = make_float3(
        __uint_as_float( optixGetAttribute_0() ),
        __uint_as_float( optixGetAttribute_1() ),
        __uint_as_float( optixGetAttribute_2() )
    );

    const float3 world_normal = normalize( optixTransformNormalFromObjectToWorldSpace( object_normal ) );
    const float3 ffnormal     = faceforward( world_normal, -optixGetWorldRayDirection(), world_normal );
    phongShade( Kd, Ka, Ks, Kr, phong_exp, ffnormal );
}


//------------------------------------------------------------------------------
//
//  Glass shading
//
//------------------------------------------------------------------------------

extern "C" __global__ void __closesthit__glass_radiance()
{
    const HitGroupData* sbt_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    const sutil::MaterialData::Glass& glass = sbt_data->material_data.glass;

    float3 object_normal = make_float3( __uint_as_float( optixGetAttribute_0() ), __uint_as_float( optixGetAttribute_1() ),
                                        __uint_as_float( optixGetAttribute_2() ) );
    object_normal        = normalize( object_normal );

    // intersection vectors
    const float3 n        = normalize( optixTransformNormalFromObjectToWorldSpace( object_normal ) );  // normal
    const float3 ray_orig = optixGetWorldRayOrigin();
    const float3 ray_dir  = optixGetWorldRayDirection();  // incident direction
    const float  ray_t    = optixGetRayTmax();
    float3       t;  // transmission direction
    float3       r;  // reflection direction

    float3                      hit_point       = ray_orig + ray_t * ray_dir;
    sutil::SphereShell::HitType hit_type        = static_cast<sutil::SphereShell::HitType>( optixGetHitKind() );
    float3                      front_hit_point = hit_point, back_hit_point = hit_point;

    if( hit_type & sutil::SphereShell::HIT_OUTSIDE_FROM_OUTSIDE || hit_type & sutil::SphereShell::HIT_INSIDE_FROM_INSIDE )
    {
        front_hit_point += params.scene_epsilon * object_normal;
        back_hit_point -= params.scene_epsilon * object_normal;
    }
    else
    {
        front_hit_point -= params.scene_epsilon * object_normal;
        back_hit_point += params.scene_epsilon * object_normal;
    }

    const float3 fhp = optixTransformPointFromObjectToWorldSpace( front_hit_point );
    const float3 bhp = optixTransformPointFromObjectToWorldSpace( back_hit_point );

    float  reflection = 1.0f;
    float3 result     = make_float3( 0.0f );

    float3 beer_attenuation;
    if( dot( n, ray_dir ) > 0 )
    {
        // Beer's law attenuation
        beer_attenuation = expf( glass.extinction_constant * ray_t );
    }
    else
    {
        beer_attenuation = make_float3( 1 );
    }

    // refraction
    const float importance = payloadGetImportance();
    if( optixGetRemainingTraceDepth() > ( params.max_depth - 1 - glass.refraction_maxdepth ) )
    {
        if( sutil::refract( t, ray_dir, n, glass.refraction_index ) )
        {
            // check for external or internal reflection
            float cos_theta = dot( ray_dir, n );
            if( cos_theta < 0.0f )
                cos_theta = -cos_theta;
            else
                cos_theta = dot( t, n );

            reflection = sutil::fresnel_schlick( cos_theta, glass.fresnel_exponent, glass.fresnel_minimum, glass.fresnel_maximum );

            float new_importance =
                importance * ( 1.0f - reflection ) * sutil::luminance( glass.refraction_color * beer_attenuation );
            float3 color = glass.cutoff_color;
            if( new_importance > glass.importance_cutoff )
            {
                color = traceRadianceRay( bhp, t, new_importance );
            }
            result += ( 1.0f - reflection ) * glass.refraction_color * color;
        }
        // else TIR
    }  // else reflection==1 so refraction has 0 weight

    // reflection
    // compare depth to max_depth - 1 to leave room for a potential shadow ray trace
    float3 color = glass.cutoff_color;
    if( optixGetRemainingTraceDepth() > ( params.max_depth - 1 - glass.reflection_maxdepth ) )
    {
        r = reflect( ray_dir, n );

        float new_importance = importance * reflection * sutil::luminance( glass.reflection_color * beer_attenuation );
        if( new_importance > glass.importance_cutoff )
        {
            color = traceRadianceRay( fhp, r, new_importance );
        }
    }

    result += reflection * glass.reflection_color * color;
    result = result * beer_attenuation;
    payloadSetResult(result);
}


extern "C" __global__ void __anyhit__glass_occlusion()
{
    const HitGroupData* sbt_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    const sutil::MaterialData::Glass& glass = sbt_data->material_data.glass;

    float3 object_normal = make_float3(
        __uint_as_float( optixGetAttribute_0() ),
        __uint_as_float( optixGetAttribute_1() ),
        __uint_as_float( optixGetAttribute_2() )
    );

    float3 result = payloadGetResult();

    float3 world_normal = normalize( optixTransformNormalFromObjectToWorldSpace( object_normal ) );
    float  nDi          = fabs( dot( world_normal, optixGetWorldRayDirection() ) );

    result *= 1 - sutil::fresnel_schlick( nDi, 5, 1 - glass.shadow_attenuation, make_float3( 1 ) );
    payloadSetResult(result);

    // Test the attenuation of the light from the glass shell
    if( sutil::luminance( result ) < glass.importance_cutoff )
        // The attenuation is so high, > 99% blocked, that we can consider testing to be done.
        optixTerminateRay();
    else
        // There is still some light coming through the glass shell that we should test other occluders.
        // We "ignore" the intersection with the glass shell, meaning that shadow testing will continue.
        // If the ray does not hit another occluder, the light's attenuation from this glass shell
        // (along with other glass shells) is then used.
        optixIgnoreIntersection();
}


//------------------------------------------------------------------------------
//
//  Metal shading
//
//------------------------------------------------------------------------------

extern "C" __global__ void __closesthit__metal_radiance()
{
    const HitGroupData* sbt_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    const sutil::MaterialData::Phong& phong = sbt_data->material_data.metal;

    float3 object_normal = make_float3(
        __uint_as_float( optixGetAttribute_0() ),
        __uint_as_float( optixGetAttribute_1() ),
        __uint_as_float( optixGetAttribute_2() )
    );

    const float3 world_normal = normalize( optixTransformNormalFromObjectToWorldSpace( object_normal ) );
    const float3 ffnormal     = faceforward( world_normal, -optixGetWorldRayDirection(), world_normal );
    phongShade( phong.Kd, phong.Ka, phong.Ks, phong.Kr, phong.phong_exp, ffnormal );
}
