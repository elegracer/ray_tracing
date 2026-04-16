/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <optix.h>

#include "optixHair.h"

#include <string>
#include <vector>


// forward declarations
class Context;
class ProgramGroups;


class Head
{
  public:
    Head( const OptixDeviceContext context, const std::string& fileName );
    ~Head();

    virtual OptixTraversableHandle traversable() const;

    virtual void gatherProgramGroups( HairProgramGroups* pProgramGroups ) const;

    virtual std::vector<HitRecord> sbtHitRecords( const ProgramGroups& programs, size_t rayTypes ) const;

    size_t numberOfVertices() const { return m_vertices; }

    size_t numberOfTriangles() const { return m_triangles; }

    virtual sutil::Aabb aabb() const { return m_aabb; }

    virtual unsigned int usesPrimitiveTypes() const { return OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE; }

  private:

    size_t                         m_vertices = 0;
    size_t                         m_triangles     = 0;
    CUdeviceptr                    m_buffer = 0;
    sutil::Aabb                    m_aabb;
    mutable OptixTraversableHandle m_hGAS            = 0;
    mutable CUdeviceptr            m_deviceBufferGAS = 0;
    sutil::TriangleMesh            m_triangleMesh;

    friend std::ostream& operator<<( std::ostream& o, const Head& head );
};

// Ouput operator for Head
std::ostream& operator<<( std::ostream& o, const Head& head );
