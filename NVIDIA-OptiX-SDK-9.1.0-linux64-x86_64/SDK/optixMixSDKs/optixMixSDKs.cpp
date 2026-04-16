/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Calls an optix API function using the current optix SDK
void runSDKCurrent();

// Calls an optix API function using the optix SDK 7.5.0
void runSDK750();

int main( int argc, char* argv[] )
{
    // Multiple distinct SDKs can be mixed within a single application
    // as long as each translation unit use only a single SDK.
    // Multiple translation units may use the same SDK version.
    // Each used SDK must be initialized exactly once across all
    // translation units using the SDK.
    // Note, mixing multiple SDKs is only supported since SDK 8.1.0
    // Mixing multiple 8.0.0 or older SDKs is not supported.
    
    // Run some code using the current optix SDK
    runSDKCurrent();
    
    // Run some code using the optix SDK 7.5.0
    runSDK750();

    return 0;
}
