/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <libntc/ntc.h>
#include <libntc/shaders/InferenceConstants.h>


struct NtcTextureSet
{
    NtcTextureSetConstants constants;

    uint8_t* d_latents;
    uint8_t* d_mlpWeights;

    int networkVersion;
    int numSubTextures;
    int totalChannels;

    // Host only data
    ntc::ITextureSetMetadata* metadata;
};
