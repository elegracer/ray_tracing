/*
 * SPDX-FileCopyrightText: Copyright (c) 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "libcache.h"
#include <string>
#include <mutex>
#include <cstring>
#include <sstream>
#include <cstdlib>

#include <zmq.h>

// ZMQ connection data
struct ZmqConnection
{
    void*       context = nullptr;
    void*       socket  = nullptr;
    std::mutex  zmqMutex;
    bool        initialized = false;
    std::string endpoint;  // Will be set by cache_connect
};

// Global ZMQ connection instance
static ZmqConnection g_zmq;

// Helper function to send ZMQ request and receive response
static std::string zmq_request( const std::string& command )
{
    if( !g_zmq.initialized || !g_zmq.socket )
        return "ERROR: Not initialized";

    // Send request
    zmq_msg_t request;
    zmq_msg_init_size( &request, command.size() );
    memcpy( zmq_msg_data( &request ), command.c_str(), command.size() );
    int rc = zmq_msg_send( &request, g_zmq.socket, 0 );
    zmq_msg_close( &request );

    if( rc == -1 )
        return "ERROR: Failed to send request";

    // Receive response
    zmq_msg_t reply;
    zmq_msg_init( &reply );
    rc = zmq_msg_recv( &reply, g_zmq.socket, 0 );

    if( rc == -1 )
    {
        zmq_msg_close( &reply );
        return "ERROR: Failed to receive response";
    }

    std::string response( static_cast<char*>( zmq_msg_data( &reply ) ), zmq_msg_size( &reply ) );
    zmq_msg_close( &reply );

    return response;
}


extern "C" {

CACHE_API bool cache_isOpen( CacheDeviceContext context )
{
    if( context == nullptr ) return false;

    std::lock_guard<std::mutex> lock( g_zmq.zmqMutex );
    return g_zmq.initialized ? true : false;
}

CACHE_API CacheResult cache_query( CacheDeviceContext context, const char* key, size_t keySize, char* value, size_t* valueSize )
{
    if( context == nullptr || key == nullptr || valueSize == nullptr )
        return CACHE_ERROR_INVALID_ARGUMENT;

    std::lock_guard<std::mutex> lock( g_zmq.zmqMutex );

    if( !g_zmq.initialized )
        return CACHE_ERROR_NOT_INITIALIZED;

    std::string keyStr( key, keySize );

    if( value == nullptr )
    {
        // First call: just get the size
        std::string command  = "SIZE " + keyStr;
        std::string response = zmq_request( command );

        if( response.substr( 0, 3 ) == "OK " )
        {
            std::string sizeStr = response.substr( 3 );
            *valueSize          = std::stoul( sizeStr );
            return CACHE_SUCCESS;
        }
        else if( response == "NOT_FOUND" )
        {
            *valueSize = 0;
            return CACHE_ERROR_KEY_NOT_FOUND;
        }
        else
        {
            return CACHE_ERROR_UNKNOWN;
        }
    }
    else
    {
        // Second call: get the actual data
        std::string command  = "GET " + keyStr;
        std::string response = zmq_request( command );

        if( response.substr( 0, 3 ) == "OK " )
        {
            std::string valueStr = response.substr( 3 );

            if( *valueSize < valueStr.size() )
            {
                *valueSize = valueStr.size();
                return CACHE_ERROR_BUFFER_TOO_SMALL;
            }

            std::memcpy( value, valueStr.c_str(), valueStr.size() );
            *valueSize = valueStr.size();
            return CACHE_SUCCESS;
        }
        else if( response == "NOT_FOUND" )
        {
            *valueSize = 0;
            return CACHE_ERROR_KEY_NOT_FOUND;
        }
        else
        {
            return CACHE_ERROR_UNKNOWN;
        }
    }
}

CACHE_API CacheResult cache_insert( CacheDeviceContext context, const char* key, size_t keySize, const char* value, size_t valueSize )
{
    if( context == nullptr || key == nullptr || value == nullptr )
        return CACHE_ERROR_INVALID_ARGUMENT;

    std::lock_guard<std::mutex> lock( g_zmq.zmqMutex );

    if( !g_zmq.initialized )
        return CACHE_ERROR_NOT_INITIALIZED;

    // Build SET command
    std::string keyStr( key, keySize );
    std::string valueStr( value, valueSize );
    std::string command = "SET " + keyStr + " " + valueStr;

    // Send request to cache server
    std::string response = zmq_request( command );

    // Parse response
    if( response == "OK" )
        return CACHE_SUCCESS;
    else
        return CACHE_ERROR_UNKNOWN;
}

CACHE_API CacheResult cache_connect( CacheDeviceContext context, const char* endpoint, int timeoutMs )
{
    if( context == nullptr )
        return CACHE_ERROR_INVALID_CONTEXT;

    if( endpoint == nullptr || endpoint[0] == '\0' )
        return CACHE_ERROR_INVALID_ARGUMENT;

    if( timeoutMs <= 0 )
        return CACHE_ERROR_INVALID_ARGUMENT;

    std::lock_guard<std::mutex> lock( g_zmq.zmqMutex );

    if( g_zmq.initialized )
        return CACHE_ERROR_ALREADY_INITIALIZED;

    // Set the endpoint
    g_zmq.endpoint = endpoint;

    // Create ZMQ context and socket
    g_zmq.context = zmq_ctx_new();
    if( !g_zmq.context )
        return CACHE_ERROR_OUT_OF_MEMORY;

    g_zmq.socket = zmq_socket( g_zmq.context, ZMQ_REQ );
    if( !g_zmq.socket )
    {
        zmq_ctx_destroy( g_zmq.context );
        g_zmq.context = nullptr;
        return CACHE_ERROR_OUT_OF_MEMORY;
    }

    // Set socket timeout using provided value
    zmq_setsockopt( g_zmq.socket, ZMQ_RCVTIMEO, &timeoutMs, sizeof( timeoutMs ) );
    zmq_setsockopt( g_zmq.socket, ZMQ_SNDTIMEO, &timeoutMs, sizeof( timeoutMs ) );

    // Connect to cache server
    if( 0 != zmq_connect( g_zmq.socket, g_zmq.endpoint.c_str() ) )
    {
        zmq_close( g_zmq.socket );
        zmq_ctx_destroy( g_zmq.context );
        g_zmq.socket  = nullptr;
        g_zmq.context = nullptr;
        return CACHE_ERROR_INVALID_OPERATION;
    }

    // Test connection with PING (don't use zmq_request since g_zmq.initialized is still false)
    const std::string ping_cmd = "PING";
    zmq_msg_t         request;
    zmq_msg_init_size( &request, ping_cmd.size() );
    memcpy( zmq_msg_data( &request ), ping_cmd.c_str(), ping_cmd.size() );

    const int send_rc = zmq_msg_send( &request, g_zmq.socket, 0 );
    zmq_msg_close( &request );

    if( send_rc == -1 )
    {
        // Set linger to 0 to avoid blocking on close
        int linger = 0;
        zmq_setsockopt( g_zmq.socket, ZMQ_LINGER, &linger, sizeof( linger ) );
        zmq_close( g_zmq.socket );
        zmq_ctx_destroy( g_zmq.context );
        g_zmq.socket  = nullptr;
        g_zmq.context = nullptr;
        return CACHE_ERROR_INVALID_OPERATION;
    }

    // Receive PONG response
    zmq_msg_t reply;
    zmq_msg_init( &reply );

    if( -1 == zmq_msg_recv( &reply, g_zmq.socket, 0 ) )
    {
        zmq_msg_close( &reply );
        // Set linger to 0 to avoid blocking on close
        int linger = 0;
        zmq_setsockopt( g_zmq.socket, ZMQ_LINGER, &linger, sizeof( linger ) );
        zmq_close( g_zmq.socket );
        zmq_ctx_destroy( g_zmq.context );
        g_zmq.socket  = nullptr;
        g_zmq.context = nullptr;
        return CACHE_ERROR_INVALID_OPERATION;
    }

    std::string response( static_cast<char*>( zmq_msg_data( &reply ) ), zmq_msg_size( &reply ) );
    zmq_msg_close( &reply );

    if( response != "PONG" )
    {
        // Set linger to 0 to avoid blocking on close
        int linger = 0;
        zmq_setsockopt( g_zmq.socket, ZMQ_LINGER, &linger, sizeof( linger ) );
        zmq_close( g_zmq.socket );
        zmq_ctx_destroy( g_zmq.context );
        g_zmq.socket  = nullptr;
        g_zmq.context = nullptr;
        return CACHE_ERROR_INVALID_OPERATION;
    }

    g_zmq.initialized = true;
    return CACHE_SUCCESS;
}

CACHE_API CacheResult cache_disconnect( CacheDeviceContext context )
{
    if( context == nullptr )
        return CACHE_ERROR_INVALID_CONTEXT;

    std::lock_guard<std::mutex> lock( g_zmq.zmqMutex );

    if( !g_zmq.initialized )
        return CACHE_ERROR_NOT_INITIALIZED;

    // Close ZMQ socket and context
    if( g_zmq.socket )
    {
        // Set linger to 0 to avoid blocking on close
        int linger = 0;
        zmq_setsockopt( g_zmq.socket, ZMQ_LINGER, &linger, sizeof( linger ) );
        zmq_close( g_zmq.socket );
        g_zmq.socket = nullptr;
    }

    if( g_zmq.context )
    {
        zmq_ctx_destroy( g_zmq.context );
        g_zmq.context = nullptr;
    }

    g_zmq.initialized = false;
    return CACHE_SUCCESS;
}
} 
