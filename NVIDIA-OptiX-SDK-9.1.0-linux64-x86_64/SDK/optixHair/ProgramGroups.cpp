/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ProgramGroups.h"

#include <optix.h>
#include <optix_stubs.h>

#include <sutil/Exception.h>
#include <sutil/sutil.h>

#include <cstring>


ProgramGroups::ProgramGroups( const OptixDeviceContext    context,
                              OptixPipelineCompileOptions pipeOptions,
                              OptixProgramGroupOptions    programGroupOptions = {} )
    : m_context( context )
    , m_pipeOptions( pipeOptions )
    , m_programGroupOptions( programGroupOptions )
{
    ;
}

void ProgramGroups::add( const OptixProgramGroupDesc& programGroupDescriptor, const std::string& name )
{
    // Only add a new program group, if one with `name` doesn't yet exist.
    if( m_nameToIndex.find( name ) == m_nameToIndex.end() )
    {
        size_t last         = m_programGroups.size();
        m_nameToIndex[name] = static_cast<unsigned int>( last );
        m_programGroups.resize( last + 1 );
        OPTIX_CHECK_LOG( optixProgramGroupCreate( m_context, &programGroupDescriptor,
                                                  1,  // num program groups
                                                  &m_programGroupOptions, LOG, &LOG_SIZE, &m_programGroups[last] ) );
    }
}

const OptixProgramGroup& ProgramGroups::operator[]( const std::string& name ) const
{
    auto iter = m_nameToIndex.find( name );
    SUTIL_ASSERT( iter != m_nameToIndex.end() );
    size_t index = iter->second;
    return m_programGroups[index];
}

const OptixProgramGroup* ProgramGroups::data() const
{
    return &( m_programGroups[0] );
}

unsigned int ProgramGroups::size() const
{
    return static_cast<unsigned int>( m_programGroups.size() );
}

//
// HairProgramGroups
//
HairProgramGroups::HairProgramGroups( const OptixDeviceContext context, OptixPipelineCompileOptions pipeOptions, unsigned int buildFlags )
    : ProgramGroups( context, pipeOptions )
{
    //
    // Create modules
    //
    OptixModuleCompileOptions defaultOptions = {};
#if OPTIX_DEBUG_DEVICE_CODE
    defaultOptions.optLevel   = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
    defaultOptions.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
#endif

    size_t      inputSize = 0;
    const char* input = sutil::getInputData( OPTIX_SAMPLE_NAME, OPTIX_SAMPLE_DIR, "optixHair.cu", inputSize );
    OPTIX_CHECK_LOG( optixModuleCreate( context,
                                        &defaultOptions,
                                        &pipeOptions,
                                        input,
                                        inputSize,
                                        LOG, &LOG_SIZE,
                                        &m_shadingModule ) );

    OptixBuiltinISOptions builtinISOptions = {};
    builtinISOptions.buildFlags = buildFlags;
    if( pipeOptions.usesPrimitiveTypeFlags & OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_QUADRATIC_BSPLINE ) {
        builtinISOptions.builtinISModuleType   = OPTIX_PRIMITIVE_TYPE_ROUND_QUADRATIC_BSPLINE;
        OPTIX_CHECK( optixBuiltinISModuleGet( context, &defaultOptions, &pipeOptions, &builtinISOptions, &m_quadraticCurveModule ) );
    }
    if( pipeOptions.usesPrimitiveTypeFlags & OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CUBIC_BSPLINE ) {
        builtinISOptions.builtinISModuleType   = OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE;
        OPTIX_CHECK( optixBuiltinISModuleGet( context, &defaultOptions, &pipeOptions, &builtinISOptions, &m_cubicCurveModule ) );
    }
    if( pipeOptions.usesPrimitiveTypeFlags & OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_LINEAR ) {
        builtinISOptions.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_LINEAR;
        OPTIX_CHECK( optixBuiltinISModuleGet( context, &defaultOptions, &pipeOptions, &builtinISOptions, &m_linearCurveModule ) );
    }
    if( pipeOptions.usesPrimitiveTypeFlags & OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CATMULLROM ) {
        builtinISOptions.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_CATMULLROM;
        OPTIX_CHECK( optixBuiltinISModuleGet( context, &defaultOptions, &pipeOptions, &builtinISOptions, &m_catromCurveModule ) );
    }
    if( pipeOptions.usesPrimitiveTypeFlags & OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_QUADRATIC_BSPLINE_ROCAPS )
    {
        builtinISOptions.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_QUADRATIC_BSPLINE_ROCAPS;
        OPTIX_CHECK( optixBuiltinISModuleGet( context, &defaultOptions, &pipeOptions, &builtinISOptions, &m_quadraticCurveModule ) );
    }
    if( pipeOptions.usesPrimitiveTypeFlags & OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CUBIC_BSPLINE_ROCAPS )
    {
        builtinISOptions.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE_ROCAPS;
        OPTIX_CHECK( optixBuiltinISModuleGet( context, &defaultOptions, &pipeOptions, &builtinISOptions, &m_cubicCurveModule ) );
    }
    if( pipeOptions.usesPrimitiveTypeFlags & OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CATMULLROM_ROCAPS )
    {
        builtinISOptions.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_CATMULLROM_ROCAPS;
        OPTIX_CHECK( optixBuiltinISModuleGet( context, &defaultOptions, &pipeOptions, &builtinISOptions, &m_catromCurveModule ) );
    }
}
