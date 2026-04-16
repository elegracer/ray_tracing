/*
 * SPDX-FileCopyrightText: Copyright (c) 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string> // for size_t

#ifdef _WIN32
    // On Windows we rely on an explicit .def file for exports and explicit loading,
    // so no special decoration is required here.
    #define CACHE_API
#elif defined(__GNUC__) || defined(__clang__)
    // On Linux, mark the entry points with default visibility so they remain
    // available for dlsym() in optimized release builds, 
    // even if builds use -fvisibility=hidden or strip unused symbols
    #define CACHE_API __attribute__((visibility("default")))
#else
    #define CACHE_API
#endif

/*
 * libcache - Standalone cache API for OptiX task serialization
 * 
 * This header provides a simple cache interface that can be used by OptiX applications
 * to cache compiled modules. The implementation uses ZeroMQ to communicate with a
 * network cache server.
 */

#ifdef __cplusplus
extern "C" {
#endif

//-------------------------------
// Types
//-------------------------------

// Opaque device context handle
typedef void* CacheDeviceContext;

// Cache operation result codes
typedef enum CacheResult
{
    CACHE_SUCCESS                   = 0,  // Operation successful
    CACHE_ERROR_INVALID_CONTEXT        ,  // Invalid or null context
    CACHE_ERROR_INVALID_ARGUMENT       ,  // Invalid parameter
    CACHE_ERROR_INVALID_OPERATION      ,  // Failed to connect to cache server or invalid operation
    CACHE_ERROR_KEY_NOT_FOUND          ,  // Key not found in cache
    CACHE_ERROR_OUT_OF_MEMORY          ,  // Failed to allocate memory or create resources
    CACHE_ERROR_NOT_INITIALIZED        ,  // Cache connection not initialized
    CACHE_ERROR_ALREADY_INITIALIZED    ,  // Cache connection already initialized
    CACHE_ERROR_BUFFER_TOO_SMALL       ,  // Output buffer too small for value
    CACHE_ERROR_UNKNOWN                ,  // Server returned unexpected response or unknown error
} CacheResult;

//-------------------------------
// API Functions
//-------------------------------

// CacheDeviceContext is an opaque pointer that can optionally be used to
// identify separate OptiX contexts within a single process, e.g., multiple
// pipelines.
//
// API functions are exported with C linkage so they can be looked up via
// GetProcAddress()/dlsym() by name. CACHE_API ensures they remain visible
// even in builds that hide or strip other symbols.

CACHE_API bool        cache_isOpen    ( CacheDeviceContext context                                                                                 );
CACHE_API CacheResult cache_connect   ( CacheDeviceContext context , const char* endpoint, int    timeoutMs                                        );
CACHE_API CacheResult cache_disconnect( CacheDeviceContext context                                                                                 );
CACHE_API CacheResult cache_query     ( CacheDeviceContext context , const char* key     , size_t keySize   ,       char* value, size_t* valueSize );
CACHE_API CacheResult cache_insert    ( CacheDeviceContext context , const char* key     , size_t keySize   , const char* value, size_t  valueSize );

#ifdef __cplusplus
}
#endif
