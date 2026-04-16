/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <assert.h>
#include <cuda.h>
#include <optix_types.h>
#include <stdint.h>
#include <sutil/vec_math.h>

struct Cluster
{
    uint32_t vertexOffset = 0u;  // vertex array index of this cluster's [0, 0]-corner
    uchar2   size{ 0u, 0u };     // cluster edge segment
};

struct ClusterTessConfig
{
    uint2         clusterSize     = { 8, 8 };      // cluster edge segements for structure grid cluster, default 8 x 8
    uint2         gridDims        = { 512, 512 };  // cluster dimension per grid, default 512 x 512
    float3        mousePosInWorld = make_float3( 0.f );  // mouse pos in world allows for user interaction
    float         animationTime   = 0;
};


//////////////////////////////////////////////////////////////////////////////////
// helper functions for generating vertex and index buffer                      //
//////////////////////////////////////////////////////////////////////////////////
void generateAnimatedVertices( CUstream& stream, const ClusterTessConfig tessConfig, CUdeviceptr& d_outVertices );

void generateIndices( CUstream& stream, const ClusterTessConfig tessConfig, CUdeviceptr& d_outIndices );


//////////////////////////////////////////////////////////////////////////////////
// helper functions for building templates, CLAS, and BLAS                      //
//////////////////////////////////////////////////////////////////////////////////
void calculateClasOutputBufferSizeAndOffsets( CUstream&       stream,
                                              const uint32_t  clusterCount,
                                              const uint32_t  g_maxClusterEdgeSegments,
                                              const uchar2*   d_edgeSegments,
                                              const uint32_t* d_templateSizes,
                                              const size_t    strideInBytes,
                                              size_t*         d_outputSizeInBytes,
                                              size_t*         d_clasAddressOffsets );

void assignExplicitAddresses( CUstream&         stream,
                              const uint32_t    clusterCount,
                              const size_t*     d_clasAddressOffsets,
                              const CUdeviceptr d_clasBuffer,
                              CUdeviceptr*      d_clasPtrsBuffer );

void makeClusters( CUstream& stream, const ClusterTessConfig tessConfig, Cluster* d_outClusters );

void makeTemplatesArgsDataForGetSizes( CUstream&                                 stream,
                                       const CUdeviceptr*                        d_templateAddresses,
                                       const uint32_t                            numTemplates,
                                       OptixClusterAccelBuildInputTemplatesArgs* d_templatesArgs );

void makeTemplatesArgsData( CUstream&                                 stream,
                            const Cluster*                            d_clusters,
                            const uint32_t                            clusterCount, 
                            const uint32_t                            g_maxClusterEdgeSegments,
                            const CUdeviceptr*                        d_templateAddresses,
                            const float3*                             d_vertexPositions,
                            OptixClusterAccelBuildInputTemplatesArgs* d_templatesArgs );

void makeClustersArgsData( CUstream&                                stream,
                           const size_t*                            d_clusterOffsets,
                           const uint32_t                           instanceCount,
                           const CUdeviceptr*                       d_clasPtrs,
                           OptixClusterAccelBuildInputClustersArgs* d_clustersArgs );

void copyGasHandlesToInstanceArray( CUstream& stream, OptixInstance* d_instances, const CUdeviceptr* d_gasHandles, uint32_t instanceCount );
