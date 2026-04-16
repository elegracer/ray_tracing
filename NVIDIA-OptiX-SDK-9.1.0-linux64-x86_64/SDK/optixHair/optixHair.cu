/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>

#include "curve.h"
#include "optixHair.cuh"

#include <sutil/cuda/GeometryData.h>
#include <sutil/cuda/LocalGeometry.h>
#include <sutil/cuda/LocalShading.h>
#include <sutil/cuda/camera.h>
#include <sutil/cuda/helpers.h>
#include <sutil/cuda/microfacet.h>

#include <sutil/vec_math.h>

extern "C"
{
__constant__ LaunchParams params;
}



__forceinline__ __device__ void setPayloadResult( float3 p )
{
    optixSetPayload_0( __float_as_uint( p.x ) );
    optixSetPayload_1( __float_as_uint( p.y ) );
    optixSetPayload_2( __float_as_uint( p.z ) );
}

__forceinline__ __device__ float getPayloadOcclusion()
{
    return __uint_as_float( optixGetPayload_0() );
}

__forceinline__ __device__ void setPayloadOcclusion( float attenuation )
{
    optixSetPayload_0( __float_as_uint( attenuation ) );
}

__forceinline__ __device__ void setPayloadOcclusionCommit()
{
    // set the sign
    optixSetPayload_0( optixGetPayload_0() | 0x80000000 );
}


// Get curve hit-point in world coordinates.
static __forceinline__ __device__ float3 getHitPoint()
{
    const float  t            = optixGetRayTmax();
    const float3 rayOrigin    = optixGetWorldRayOrigin();
    const float3 rayDirection = optixGetWorldRayDirection();

    return rayOrigin + t * rayDirection;
}

static __forceinline__ __device__ void traceRadiance(
        OptixTraversableHandle      handle,
        float3                      ray_origin,
        float3                      ray_direction,
        float                       tmin,
        float                       tmax,
        PayloadRadiance*   payload
        )
{
    unsigned int u0 = 0; // output only
    unsigned int u1 = 0; // output only
    unsigned int u2 = 0; // output only
    optixTrace(
            handle,
            ray_origin, ray_direction,
            tmin,
            tmax,
            0.0f,                     // rayTime
            OptixVisibilityMask( 1 ),
            OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
            RAY_TYPE_RADIANCE,        // SBT offset
            RAY_TYPE_COUNT,           // SBT stride
            RAY_TYPE_RADIANCE,        // missSBTIndex
            u0, u1, u2 );

     payload->result.x = __uint_as_float( u0 );
     payload->result.y = __uint_as_float( u1 );
     payload->result.z = __uint_as_float( u2 );
}



static __forceinline__ __device__ float traceOcclusion(
        OptixTraversableHandle handle,
        float3                 ray_origin,
        float3                 ray_direction,
        float                  tmin,
        float                  tmax
        )
{
    // Introduce the concept of 'pending' and 'committed' attenuation.
    // This avoids the usage of closesthit shaders and allows the usage of the OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT flag.
    // The attenuation is marked as pending with a positive sign bit and marked committed by switching the sign bit.
    // Attenuation magnitude can be changed in anyhit programs and stays pending.
    // The final attenuation gets committed in the miss shader (by setting the sign bit).
    // If no miss shader is invoked (traversal was terminated due to an opaque hit)
    // the attenuation is not committed and the ray is deemed fully occluded.
    unsigned int attenuation = __float_as_uint(1.f);
    optixTrace(
            handle,
            ray_origin,
            ray_direction,
            tmin,
            tmax,
            0.0f,                    // rayTime
            OptixVisibilityMask( 1 ),
            OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT | OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT,
            RAY_TYPE_OCCLUSION,      // SBT offset
            RAY_TYPE_COUNT,          // SBT stride
            RAY_TYPE_OCCLUSION,      // missSBTIndex
            attenuation );

    // committed attenuation is negated
    return fmaxf(0, -__uint_as_float(attenuation));
}

// Compute surface normal of quadratic primitive in world space.
static __forceinline__ __device__ float3 normalLinear( const int primitiveIndex )
{
    const OptixTraversableHandle gas         = optixGetGASTraversableHandle();
    const unsigned int           gasSbtIndex = optixGetSbtGASIndex();
    float4                       controlPoints[2];

    optixGetLinearCurveVertexData( gas, primitiveIndex, gasSbtIndex, 0.0f, controlPoints );

    LinearInterpolator interpolator;
    interpolator.initialize( controlPoints );

    float3 hitPoint = getHitPoint();
    // interpolators work in object space
    hitPoint            = optixTransformPointFromWorldToObjectSpace( hitPoint );
    const float3 normal = surfaceNormal( interpolator, optixGetCurveParameter(), hitPoint );
    return optixTransformNormalFromObjectToWorldSpace( normal );
}

// Compute surface normal of quadratic primitive in world space.
static __forceinline__ __device__ float3 normalQuadratic( const int primitiveIndex )
{
    const OptixTraversableHandle gas         = optixGetGASTraversableHandle();
    const unsigned int           gasSbtIndex = optixGetSbtGASIndex();
    float4                       controlPoints[3];

    optixGetQuadraticBSplineVertexData( gas, primitiveIndex, gasSbtIndex, 0.0f, controlPoints );

    QuadraticInterpolator interpolator;
    interpolator.initializeFromBSpline( controlPoints );

    float3 hitPoint = getHitPoint();
    // interpolators work in object space
    hitPoint            = optixTransformPointFromWorldToObjectSpace( hitPoint );
    const float3 normal = surfaceNormal( interpolator, optixGetCurveParameter(), hitPoint );
    return optixTransformNormalFromObjectToWorldSpace( normal );
}

// Compute surface normal of quadratic rocaps primitive in world space.
static __forceinline__ __device__ float3 normalQuadraticRocaps( const int primitiveIndex )
{
    const OptixTraversableHandle gas         = optixGetGASTraversableHandle();
    const unsigned int           gasSbtIndex = optixGetSbtGASIndex();
    float4                       controlPoints[3];

    optixGetQuadraticBSplineRocapsVertexDataFromHandle( gas, primitiveIndex, gasSbtIndex, 0.0f, controlPoints );

    QuadraticInterpolator interpolator;
    interpolator.initializeFromBSpline( controlPoints );

    float  u         = optixGetCurveParameter();
    float3 curve_pos = interpolator.position3( u );

    float3 hitPoint = getHitPoint();
    // interpolators work in object space
    hitPoint            = optixTransformPointFromWorldToObjectSpace( hitPoint );
    const float3 normal = normalize( hitPoint - curve_pos );
    return optixTransformNormalFromObjectToWorldSpace( normal );
}

// Compute surface normal of cubic b-spline primitive in world space.
static __forceinline__ __device__ float3 normalCubic( const int primitiveIndex )
{
    const OptixTraversableHandle gas         = optixGetGASTraversableHandle();
    const unsigned int           gasSbtIndex = optixGetSbtGASIndex();
    float4                       controlPoints[4];

    optixGetCubicBSplineVertexData( gas, primitiveIndex, gasSbtIndex, 0.0f, controlPoints );

    CubicInterpolator interpolator;
    interpolator.initializeFromBSpline( controlPoints );

    float3 hitPoint = getHitPoint();
    // interpolators work in object space
    hitPoint            = optixTransformPointFromWorldToObjectSpace( hitPoint );
    const float3 normal = surfaceNormal( interpolator, optixGetCurveParameter(), hitPoint );
    return optixTransformNormalFromObjectToWorldSpace( normal );
}

// Compute surface normal of cubic rocaps b-spline primitive in world space.
static __forceinline__ __device__ float3 normalCubicRocaps( const int primitiveIndex )
{
    const OptixTraversableHandle gas         = optixGetGASTraversableHandle();
    const unsigned int           gasSbtIndex = optixGetSbtGASIndex();
    float4                       controlPoints[4];

    optixGetCubicBSplineRocapsVertexDataFromHandle( gas, primitiveIndex, gasSbtIndex, 0.0f, controlPoints );

    CubicInterpolator interpolator;
    interpolator.initializeFromBSpline( controlPoints );

    float  u         = optixGetCurveParameter();
    float3 curve_pos = interpolator.position3( u );

    float3 hitPoint = getHitPoint();
    // interpolators work in object space
    hitPoint            = optixTransformPointFromWorldToObjectSpace( hitPoint );
    const float3 normal = normalize( hitPoint - curve_pos );
    return optixTransformNormalFromObjectToWorldSpace( normal );
}

// Compute surface normal of Catmull-Rom primitive in world space.
static __forceinline__ __device__ float3 normalCatrom( const int primitiveIndex )
{
    const OptixTraversableHandle gas         = optixGetGASTraversableHandle();
    const unsigned int           gasSbtIndex = optixGetSbtGASIndex();
    float4                       controlPoints[4];

    optixGetCatmullRomVertexData( gas, primitiveIndex, gasSbtIndex, 0.0f, controlPoints );

    CubicInterpolator interpolator;
    interpolator.initializeFromCatrom( controlPoints );

    float3 hitPoint = getHitPoint();
    // interpolators work in object space
    hitPoint            = optixTransformPointFromWorldToObjectSpace( hitPoint );
    const float3 normal = surfaceNormal( interpolator, optixGetCurveParameter(), hitPoint );
    return optixTransformNormalFromObjectToWorldSpace( normal );
}

// Compute surface normal of Catmull-Rom rocaps primitive in world space.
static __forceinline__ __device__ float3 normalCatromRocaps( const int primitiveIndex )
{
    const OptixTraversableHandle gas         = optixGetGASTraversableHandle();
    const unsigned int           gasSbtIndex = optixGetSbtGASIndex();
    float4                       controlPoints[4];

    optixGetCatmullRomRocapsVertexDataFromHandle( gas, primitiveIndex, gasSbtIndex, 0.0f, controlPoints );

    CubicInterpolator interpolator;
    interpolator.initializeFromCatrom( controlPoints );

    float  u         = optixGetCurveParameter();
    float3 curve_pos = interpolator.position3( u );

    float3 hitPoint = getHitPoint();
    // interpolators work in object space
    hitPoint            = optixTransformPointFromWorldToObjectSpace( hitPoint );
    const float3 normal = normalize( hitPoint - curve_pos );
    return optixTransformNormalFromObjectToWorldSpace( normal );
}

static __forceinline__ __device__ float3 shade(
    const HitGroupData* hitGroupData,
    const float3        hitPoint,
    const float3        normal,
    const float3        base_color
)
{
    //
    // Retrieve material data
    //
    float metallic  = hitGroupData->material_data.pbr.metallic;
    float roughness = hitGroupData->material_data.pbr.roughness;

    //
    // Convert to material params
    //
    const float  F0         = 0.04f;
    const float3 diff_color = base_color * ( 1.0f - F0 ) * ( 1.0f - metallic );
    const float3 spec_color = lerp( make_float3( F0 ), base_color, metallic );
    const float  alpha      = roughness * roughness;

    float3 result = make_float3( 0.0f );

    for( int i = 0; i < params.lights.count; ++i )
    {
        sutil::Light light = params.lights[i];

        if( light.type == sutil::Light::Type::POINT )
        {
            const float  L_dist  = length( light.point.position - hitPoint );
            const float3 L       = ( light.point.position - hitPoint ) / L_dist;
            const float3 V       = -normalize( optixGetWorldRayDirection() );
            const float3 H       = normalize( L + V );
            const float  N_dot_L = dot( normal, L );
            const float  N_dot_V = dot( normal, V );
            const float  N_dot_H = dot( normal, H );
            const float  V_dot_H = dot( V, H );

            if( N_dot_L > 0.0f && N_dot_V > 0.0f )
            {
                const float tmin        = 0.001f;           // TODO
                const float tmax        = L_dist - 0.001f;  // TODO
                const float attenuation = traceOcclusion( params.handle, hitPoint, L, tmin, tmax );
                if( attenuation > 0.f )
                {
                    const float3 F     = sutil::schlick( spec_color, V_dot_H );
                    const float  G_vis = sutil::vis( N_dot_L, N_dot_V, alpha );
                    const float  D     = sutil::ggxNormal( N_dot_H, alpha );

                    const float3 diff = ( 1.0f - F ) * diff_color / M_PIf;
                    const float3 spec = F * G_vis * D;

                    result += light.point.color * attenuation * light.point.intensity * N_dot_L * ( diff + spec );
                }
            }
        }
    }

    return result;
}

// Get u-parameter for full strand.
//
// Parameters:
//    geo - the GeometricData from the SBT.
//    primitiveIndex - the primitive index
//
static __forceinline__ __device__ float getStrandU( const sutil::GeometryData& geo, const int primitiveIndex )
{
    float  segmentU   = optixGetCurveParameter();
    float2 strandInfo = geo.getCurveArray().strand_u[primitiveIndex];
    // strandInfo.x ~ strand u at segment start
    // strandInfo.y ~ scale factor (i.e. 1/numberOfSegments)
    return strandInfo.x + segmentU * strandInfo.y;
}

// Compute normal
//
static __forceinline__ __device__ float3 computeNormal( OptixPrimitiveType type, const int primitiveIndex )
{
    switch( type )
    {
        case OPTIX_PRIMITIVE_TYPE_ROUND_LINEAR:
            return normalLinear( primitiveIndex );
        case OPTIX_PRIMITIVE_TYPE_ROUND_QUADRATIC_BSPLINE:
            return normalQuadratic( primitiveIndex );
        case OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE:
            return normalCubic( primitiveIndex );
        case OPTIX_PRIMITIVE_TYPE_ROUND_CATMULLROM:
            return normalCatrom( primitiveIndex );
        case OPTIX_PRIMITIVE_TYPE_ROUND_QUADRATIC_BSPLINE_ROCAPS:
            return normalQuadraticRocaps( primitiveIndex );
        case OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE_ROCAPS:
            return normalCubicRocaps( primitiveIndex );
        case OPTIX_PRIMITIVE_TYPE_ROUND_CATMULLROM_ROCAPS:
            return normalCatromRocaps( primitiveIndex );
    }
    return make_float3( 0.0f );
}

extern "C" __global__ void __closesthit__curve_strand_u()
{
    const unsigned int primitiveIndex = optixGetPrimitiveIndex();

    const HitGroupData*  hitGroupData = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    const sutil::GeometryData&  geometryData = hitGroupData->geometry_data;

    const float3 normal     = computeNormal( optixGetPrimitiveType(), primitiveIndex );
    const float3 colors[2]  = { make_float3( 1, 0, 0 ), make_float3( 0, 1, 0 ) };
    const float  u          = getStrandU( geometryData, primitiveIndex );
    const float3 base_color = colors[0] * u + colors[1] * ( 1 - u );

    const float3 hitPoint = getHitPoint();
    const float3 result   = shade( hitGroupData, hitPoint, normal, base_color );

    setPayloadResult( result );
}

extern "C" __global__ void __closesthit__curve_segment_u()
{
    const unsigned int       primitiveIndex = optixGetPrimitiveIndex();
    const OptixPrimitiveType primitiveType  = optixGetPrimitiveType();

    const bool rocaps = ( primitiveType == OPTIX_PRIMITIVE_TYPE_ROUND_QUADRATIC_BSPLINE || OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE
                          || primitiveType == OPTIX_PRIMITIVE_TYPE_ROUND_CATMULLROM_ROCAPS );

    const HitGroupData* hitGroupData = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );

    const float3 normal    = computeNormal( primitiveType, primitiveIndex );
    const float3 colors[3] = { make_float3( 1, 0, 0 ), make_float3( 0, 1, 0 ), make_float3( 0, 0, 1 ) };
    const float  u         = optixGetCurveParameter();
    float3       base_color;
    if( ( u == 0.0f || u == 1.0f ) && !rocaps )  // on end-cap
        base_color = colors[2];
    else
        base_color = colors[0] * u + colors[1] * ( 1 - u );

    const float3 hitPoint = getHitPoint();
    const float3 result   = shade( hitGroupData, hitPoint, normal, base_color );

    setPayloadResult( result );
}

extern "C" __global__ void __closesthit__curve_strand_idx()
{
    unsigned int primitiveIndex = optixGetPrimitiveIndex();

    const HitGroupData* hitGroupData = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    const sutil::GeometryData&  geometryData = hitGroupData->geometry_data;

    float3       normal      = computeNormal( optixGetPrimitiveType(), primitiveIndex );
    float3       colors[6]   = { make_float3( 1, 0, 0 ), make_float3( 0, 1, 0 ), make_float3( 0, 0, 1 ),
                                 make_float3( 1, 1, 0 ), make_float3( 1, 0, 1 ), make_float3( 0, 1, 1 ) };
    unsigned int strandIndex = geometryData.getCurveArray().strand_i[primitiveIndex];
    uint2        strandInfo  = geometryData.getCurveArray().strand_info[strandIndex];
    float        u           = ( primitiveIndex - strandInfo.x ) / (float)strandInfo.y;
    float3       base_color  = colors[0] * u + colors[strandIndex % 5 + 1] * ( 1.0f - u );

    float3 hitPoint = getHitPoint();
    float3 result   = shade( hitGroupData, hitPoint, normal, base_color );

    setPayloadResult( result );
}




//------------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------------
//
extern "C" __global__ void __raygen__pinhole()
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



extern "C" __global__ void __anyhit__radiance()
{
    const HitGroupData* hit_group_data = reinterpret_cast< HitGroupData* >( optixGetSbtDataPointer() );
    if( hit_group_data->material_data.pbr.base_color_tex )
    {
        const sutil::LocalGeometry geom       = getLocalGeometry( hit_group_data->geometry_data );
        const float                base_alpha = sutil::sampleTexture<float4>( hit_group_data->material_data.pbr.base_color_tex, geom ).w;
        // force mask mode, even for blend mode, as we don't do recursive traversal.
        if( base_alpha < hit_group_data->material_data.alpha_cutoff )
            optixIgnoreIntersection();
    }
}

extern "C" __global__ void __anyhit__occlusion()
{
    const HitGroupData* hit_group_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    if( hit_group_data->material_data.pbr.base_color_tex )
    {
        const sutil::LocalGeometry geom       = getLocalGeometry( hit_group_data->geometry_data );
        const float                base_alpha = sutil::sampleTexture<float4>( hit_group_data->material_data.pbr.base_color_tex, geom ).w;

        if( hit_group_data->material_data.alpha_mode != sutil::MaterialData::ALPHA_MODE_OPAQUE )
        {
            if( hit_group_data->material_data.alpha_mode == sutil::MaterialData::ALPHA_MODE_MASK )
            {
                if( base_alpha < hit_group_data->material_data.alpha_cutoff )
                    optixIgnoreIntersection();
            }

            float attenuation = getPayloadOcclusion() * (1.f - base_alpha);
            if( attenuation > 0.f )
            {
                setPayloadOcclusion( attenuation );
                optixIgnoreIntersection();
            }
        }
    }
}

extern "C" __global__ void __miss__constant_radiance()
{
    setPayloadResult( params.miss_color );
}

extern "C" __global__ void __miss__occlusion()
{
    setPayloadOcclusionCommit();
}

extern "C" __global__ void __closesthit__radiance()
{
    const HitGroupData*        hit_group_data = reinterpret_cast<HitGroupData*>( optixGetSbtDataPointer() );
    const sutil::LocalGeometry geom           = getLocalGeometry( hit_group_data->geometry_data );

    //
    // Retrieve material data
    //
    float4 base_color = hit_group_data->material_data.pbr.base_color * geom.color;
    if( hit_group_data->material_data.pbr.base_color_tex )
    {
        const float4 base_color_tex = sutil::sampleTexture<float4>( hit_group_data->material_data.pbr.base_color_tex, geom );

        // don't gamma correct the alpha channel.
        const float3 base_color_tex_linear = sutil::linearize( make_float3( base_color_tex ) );

        base_color *= make_float4( base_color_tex_linear.x, base_color_tex_linear.y, base_color_tex_linear.z, base_color_tex.w );
    }

    float  metallic  = hit_group_data->material_data.pbr.metallic;
    float  roughness = hit_group_data->material_data.pbr.roughness;
    float4 mr_tex    = make_float4( 1.0f );
    if( hit_group_data->material_data.pbr.metallic_roughness_tex )
        // MR tex is (occlusion, roughness, metallic )
        mr_tex = sutil::sampleTexture<float4>( hit_group_data->material_data.pbr.metallic_roughness_tex, geom );
    roughness *= mr_tex.y;
    metallic *= mr_tex.z;

    //
    // Convert to material params
    //
    const float  F0         = 0.04f;
    const float3 diff_color = make_float3( base_color ) * ( 1.0f - F0 ) * ( 1.0f - metallic );
    const float3 spec_color = lerp( make_float3( F0 ), make_float3( base_color ), metallic );
    const float  alpha      = roughness * roughness;

    float3 result = make_float3( 0.0f );

    //
    // compute emission
    //

    float3 emissive_factor = hit_group_data->material_data.emissive_factor;
    float4 emissive_tex = make_float4( 1.0f );
    if( hit_group_data->material_data.emissive_tex )
        emissive_tex = sutil::sampleTexture<float4>( hit_group_data->material_data.emissive_tex, geom );
    result += emissive_factor * make_float3( emissive_tex );

    //
    // compute direct lighting
    //

    float3 N = geom.N;
    if( hit_group_data->material_data.normal_tex )
    {
        const int texcoord_idx = hit_group_data->material_data.normal_tex.texcoord;
        const float4 NN =
            2.0f * sutil::sampleTexture<float4>( hit_group_data->material_data.normal_tex, geom ) - make_float4( 1.0f );

        // Transform normal from texture space to rotated UV space.
        const float2 rotation = hit_group_data->material_data.normal_tex.texcoord_rotation;
        const float2 NN_proj  = make_float2( NN.x, NN.y );
        const float3 NN_trns  = make_float3(
            dot( NN_proj, make_float2( rotation.y, -rotation.x ) ),
            dot( NN_proj, make_float2( rotation.x,  rotation.y ) ),
            NN.z );

        N = normalize( NN_trns.x * normalize( geom.texcoord[texcoord_idx].dpdu ) + NN_trns.y * normalize( geom.texcoord[texcoord_idx].dpdv ) + NN_trns.z * geom.N );
    }

    // Flip normal to the side of the incomming ray
    if( dot( N, optixGetWorldRayDirection() ) > 0.f )
        N = -N;

    for( int i = 0; i < params.lights.count; ++i )
    {
        sutil::Light light = params.lights[i];
        if( light.type == sutil::Light::Type::POINT )
        {
            if( optixGetRemainingTraceDepth() > 0 )
            {
                // TODO: optimize
                const float  L_dist  = length( light.point.position - geom.P );
                const float3 L       = ( light.point.position - geom.P ) / L_dist;
                const float3 V       = -normalize( optixGetWorldRayDirection() );
                const float3 H       = normalize( L + V );
                const float  N_dot_L = dot( N, L );
                const float  N_dot_V = dot( N, V );
                const float  N_dot_H = dot( N, H );
                const float  V_dot_H = dot( V, H );

                if( N_dot_L > 0.0f && N_dot_V > 0.0f )
                {
                    const float tmin        = 0.001f;           // TODO
                    const float tmax        = L_dist - 0.001f;  // TODO
                    const float attenuation = traceOcclusion( params.handle, geom.P, L, tmin, tmax );
                    if( attenuation > 0.f )
                    {
                        const float3 F     = sutil::schlick( spec_color, V_dot_H );
                        const float  G_vis = sutil::vis( N_dot_L, N_dot_V, alpha );
                        const float  D     = sutil::ggxNormal( N_dot_H, alpha );

                        const float3 diff = ( 1.0f - F ) * diff_color / M_PIf;
                        const float3 spec = F * G_vis * D;

                        result += light.point.color * attenuation * light.point.intensity * N_dot_L * ( diff + spec );
                    }
                }
            }
        }
        else if( light.type == sutil::Light::Type::AMBIENT )
        {
            result += light.ambient.color * make_float3( base_color );
        }
    }

    if( hit_group_data->material_data.alpha_mode == sutil::MaterialData::ALPHA_MODE_BLEND )
    {
        result *= base_color.w;

        if( optixGetRemainingTraceDepth() > 0 )
        {
            PayloadRadiance alpha_payload;
            alpha_payload.result = make_float3( 0.0f );
            traceRadiance(
                params.handle,
                optixGetWorldRayOrigin(),
                optixGetWorldRayDirection(),
                optixGetRayTmax(),  // tmin
                1e16f,              // tmax
                &alpha_payload );

            result += alpha_payload.result * make_float3( 1.f - base_color.w );
        }
    }

    setPayloadResult( result );
}

