/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "sutil/WorkDistribution.h"

extern "C" __global__ void fillSamples(
        int   gpu_idx,
        int   num_gpus,
        int   width,
        int   height,
        int2* sample_indices )
{
    StaticWorkDistribution wd;
    wd.setRasterSize( width, height );
    wd.setNumGPUs( num_gpus );

    const int sample_idx = blockIdx.x;
    sample_indices[sample_idx] = wd.getSamplePixel( gpu_idx, sample_idx );
}


extern "C" __host__ void fillSamplesCUDA(
        int          num_samples,
        cudaStream_t stream,
        int          gpu_idx,
        int          num_gpus,
        int          width,
        int          height,
        int2*        sample_indices )
{
    fillSamples<<<num_samples, 1, 0, stream>>>(
        gpu_idx,
        num_gpus,
        width,
        height,
        sample_indices );
}
