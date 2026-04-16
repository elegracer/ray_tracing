/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <optix.h>

#include <map>
#include <string>
#include <vector>


class ProgramGroups
{
  public:
    const OptixProgramGroup* data() const;
    unsigned int             size() const;
    const OptixProgramGroup& operator[]( const std::string& name ) const;

    void add( const OptixProgramGroupDesc& programGroupDescriptor, const std::string& name );

  protected:
    ProgramGroups( const OptixDeviceContext context, OptixPipelineCompileOptions pipeOptions, OptixProgramGroupOptions programGroupOptions );

  private:
    const OptixDeviceContext    m_context;
    OptixPipelineCompileOptions m_pipeOptions;
    OptixProgramGroupOptions    m_programGroupOptions;

    std::vector<OptixProgramGroup>      m_programGroups;
    std::map<std::string, unsigned int> m_nameToIndex;
};

class HairProgramGroups : public ProgramGroups
{
  public:
    HairProgramGroups( const OptixDeviceContext context, OptixPipelineCompileOptions pipeOptions,
                       unsigned int buildFlags );

    OptixModule m_shadingModule;
    OptixModule m_quadraticCurveModule;
    OptixModule m_cubicCurveModule;
    OptixModule m_linearCurveModule;
    OptixModule m_catromCurveModule;
};
