/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <cuda.h>
#include <optix_types.h>
#include <stdint.h>
#include <sutil/vec_math.h>

struct Cluster
{
    uint32_t                       triangleCount;
    uint32_t                       vertexStrideInBytes;
    uint32_t                       vertexCount;
    uint32_t                       indexBufferStrideInBytes;
    OptixClusterAccelIndicesFormat indexFormat;
    CUdeviceptr                    d_indices;
    CUdeviceptr                    d_positions;
    CUdeviceptr                    d_deformedPositions;
};


void deformClusterVertices( CUstream& stream, const uint32_t clusterCount, const float animationTime, Cluster* d_clusters );


//////////////////////////////////////////////////////////////////////////////////
// helper functions for building templates, CLAS, and GAS                       //
//////////////////////////////////////////////////////////////////////////////////
void assignExplicitAddresses( CUstream&         stream,
                              const uint32_t    clusterCount,
                              const size_t*     d_clasAddressOffsets,
                              const CUdeviceptr d_clasBuffer,
                              CUdeviceptr*      d_clasPtrsBuffer );

void calculateClasOutputBufferSizeAndOffsets( CUstream&       stream,
                                              const uint32_t  clusterCount,
                                              const uint32_t* d_templateSizes,
                                              size_t*         d_outputSizeInBytes,
                                              size_t*         d_clasAddressOffsets );

void copyGasHandlesToInstanceArray( CUstream& stream, OptixInstance* d_instances, const CUdeviceptr* d_gasHandles, const uint32_t instanceCount );

void makeTemplatesArgsDataForGetSizes( CUstream&                                 stream,
                                       const CUdeviceptr*                        d_templateAddresses,
                                       const uint32_t                            numTemplates,
                                       OptixClusterAccelBuildInputTemplatesArgs* d_templatesArgs );

void makeTemplatesArgsData( CUstream&                                 stream,
                            const Cluster*                            d_clusters,
                            const uint32_t                            clusterCount,
                            const CUdeviceptr*                        d_templateAddresses,
                            OptixClusterAccelBuildInputTemplatesArgs* d_templatesArgs );

void makeInputTrianglesArgsData( CUstream&                                 stream,
                                 const Cluster*                            d_clusters,
                                 const uint32_t                            clusterCount,
                                 OptixClusterAccelBuildInputTrianglesArgs* d_trianglesArgs );

void makeClustersArgsData( CUstream&                                stream,
                           const size_t*                            d_clusterOffsets,
                           const uint32_t                           instanceCount,
                           const CUdeviceptr*                       d_clasPtrs,
                           OptixClusterAccelBuildInputClustersArgs* d_clustersArgs );
