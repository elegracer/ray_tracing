/*
 * SPDX-FileCopyrightText: Copyright (c) 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>
#include <optix_function_table_definition.h>
#include <optix_stack_size.h>
#include <optix_stubs.h>

#include <cuda_runtime.h>

#include <sutil/CUDAOutputBuffer.h>
#include <sutil/Exception.h>
#include <sutil/sutil.h>
#include <sampleConfig.h>

#include "optixCustomCache.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <map>
#include <vector>
#include <queue>

// For dynamic library loading
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Include cache API
#include "libcache.h"

// In-memory cache for stub behavior (when libcache is not loaded)
static std::map<std::string, std::vector<char>> g_inMemoryCache;

template <typename T>
struct SbtRecord
{
    __align__( OPTIX_SBT_RECORD_ALIGNMENT ) char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    T data;
};

typedef SbtRecord<RayGenData> RayGenSbtRecord;
typedef SbtRecord<int>        MissSbtRecord;

// Dynamic library handle and function pointers
#ifdef _WIN32
using LibraryHandle = HMODULE;
#else
using LibraryHandle = void*;
#endif

struct CacheLibrary
{
    LibraryHandle handle = nullptr;

    // Function pointers matching libcache API
    using cache_isOpen_fn     = decltype(&cache_isOpen);
    using cache_connect_fn    = decltype(&cache_connect);
    using cache_disconnect_fn = decltype(&cache_disconnect);
    using cache_query_fn      = decltype(&cache_query);
    using cache_insert_fn     = decltype(&cache_insert);
    
    cache_isOpen_fn     isOpen     = nullptr;
    cache_connect_fn    connect    = nullptr;
    cache_disconnect_fn disconnect = nullptr;
    cache_query_fn      query      = nullptr;
    cache_insert_fn     insert     = nullptr;

    bool loadLibrary( const std::string& path )
    {
#ifdef _WIN32
        handle = LoadLibraryA( path.c_str() );
        if( !handle )
        {
            std::cerr << "Failed to load library: " << path << " (Error: " << GetLastError() << ")" << std::endl;
            return false;
        }

        isOpen     = reinterpret_cast<decltype( isOpen     )>( GetProcAddress( handle, "cache_isOpen"     ) );
        connect    = reinterpret_cast<decltype( connect    )>( GetProcAddress( handle, "cache_connect"    ) );
        disconnect = reinterpret_cast<decltype( disconnect )>( GetProcAddress( handle, "cache_disconnect" ) );
        query      = reinterpret_cast<decltype( query      )>( GetProcAddress( handle, "cache_query"      ) );
        insert     = reinterpret_cast<decltype( insert     )>( GetProcAddress( handle, "cache_insert"     ) );
#else
        handle = dlopen( path.c_str(), RTLD_LAZY );
        if( !handle )
        {
            std::cerr << "Failed to load library: " << path << " (" << dlerror() << ")" << std::endl;
            return false;
        }

        isOpen     = reinterpret_cast<decltype( isOpen     )>( dlsym( handle, "cache_isOpen"     ) );
        connect    = reinterpret_cast<decltype( connect    )>( dlsym( handle, "cache_connect"    ) );
        disconnect = reinterpret_cast<decltype( disconnect )>( dlsym( handle, "cache_disconnect" ) );
        query      = reinterpret_cast<decltype( query      )>( dlsym( handle, "cache_query"      ) );
        insert     = reinterpret_cast<decltype( insert     )>( dlsym( handle, "cache_insert"     ) );
#endif

        if( !isOpen || !connect || !disconnect || !query || !insert )
        {
            std::cerr << "Failed to load all required functions from library" << std::endl;
            unloadLibrary();
            return false;
        }

        std::cout << "[Cache] Successfully loaded cache library: " << path << std::endl;
        return true;
    }

    void unloadLibrary()
    {
        if( handle )
        {
#ifdef _WIN32
            FreeLibrary( handle );
#else
            dlclose( handle );
#endif
            handle = nullptr;
        }
        isOpen     = nullptr;
        connect    = nullptr;
        disconnect = nullptr;
        query      = nullptr;
        insert     = nullptr;
    }

    ~CacheLibrary() { unloadLibrary(); }
};

// Global cache library instance
std::unique_ptr<CacheLibrary> g_cacheLib;
CacheDeviceContext g_cacheContext = nullptr;

// Helper function to connect with a spinner animation
CacheResult connectWithWaitingMessage( CacheLibrary* cacheLib, CacheDeviceContext context, const char* endpoint, int timeoutMs )
{
    std::atomic<bool> connected( false );
    CacheResult       result = CACHE_ERROR_UNKNOWN;

    // Start connection in a separate thread
    std::thread connectionThread( [&]() {
        result    = cacheLib->connect( context, endpoint, timeoutMs );
        connected = true;
    } );

    // Spinner characters
    const char spinner[]     = { '|', '/', '-', '\\' };
    int        spinnerIndex  = 0;
    bool       showedSpinner = false;

    // Wait 200ms before showing spinner
    auto startTime = std::chrono::steady_clock::now();
    while( !connected )
    {
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - startTime ).count();

        if( elapsed > 200 )
        {
            if( !showedSpinner )
            {
                std::cout << "[Cache] Waiting for cache server " << std::flush;
                showedSpinner = true;
            }
            // Update spinner
            std::cout << "\b" << spinner[spinnerIndex] << std::flush;
            spinnerIndex = ( spinnerIndex + 1 ) % 4;
        }

        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }

    // Clear spinner if it was shown
    if( showedSpinner )
    {
        std::cout << "\b \b" << std::endl;  // Erase spinner character
    }

    connectionThread.join();
    return result;
}

// Cache interface functions (use library if loaded, otherwise use in-memory cache)
bool loadFromCache( const std::string& key, std::vector<char>& data )
{
    if( g_cacheLib && g_cacheLib->handle && g_cacheLib->isOpen( g_cacheContext ) )
    {
        std::cout << "[Cache (Lib)] Query for key: " << key << std::endl;

        // Use library implementation
        size_t      valueSize = 0;
        CacheResult result    = g_cacheLib->query( g_cacheContext, key.c_str(), key.size(), nullptr, &valueSize );

        if( result == CACHE_SUCCESS && valueSize > 0 )
        {
            std::cout << "[Cache (Lib)] Hit for key: " << key << " (" << data.size() << " bytes)" << std::endl;
            data.resize( valueSize );
            result = g_cacheLib->query( g_cacheContext, key.c_str(), key.size(), data.data(), &valueSize );
            std::cout << "[Cache (Lib)] Read " << valueSize << " bytes with key: " << key << std::endl;
            return result == CACHE_SUCCESS;
        }
        return false;
    }
    else
    {
        // Stub behavior: use in-memory cache
        auto it = g_inMemoryCache.find( key );
        if( it != g_inMemoryCache.end() )
        {
            data = it->second;
            std::cout << "[Cache (Mem)] Hit for key: " << key << " (" << data.size() << " bytes)" << std::endl;
            return true;
        }
        return false;
    }
}

void saveToCache( const std::string& key, const void* data, size_t size )
{
    if( g_cacheLib && g_cacheLib->handle && g_cacheLib->isOpen( g_cacheContext ) )
    {
        // Use library implementation
        CacheResult result =
            g_cacheLib->insert( g_cacheContext, key.c_str(), key.size(), reinterpret_cast<const char*>( data ), size );
        if( result == CACHE_SUCCESS )
        {
            std::cout << "[Cache (Lib)] Wrote " << size << " bytes with key: " << key << std::endl;
        }
        else
        {
            std::cerr << "[Cache (Lib)] Failed to write " << size << " bytes with key: " << key << std::endl;
        }
    }
    else
    {
        // Stub behavior: use in-memory cache
        const char* charData = reinterpret_cast<const char*>( data );
        g_inMemoryCache[key] = std::vector<char>( charData, charData + size );
        std::cout << "[Cache (Mem)] Stored " << size << " bytes in memory with key: " << key << std::endl;
    }
}

// Custom task execution with caching support
class CachingTaskExecutor
{
    std::queue<OptixTask> m_taskQueue;

  public:
    OptixModuleCompileState m_moduleState;
    unsigned int            m_maxNumAdditionalTasks = 2;

    void executeTaskWithCaching( OptixTask task, OptixModule module )
    {
        // Get serialization key for cache lookup
        size_t keySize = 0;
        OPTIX_CHECK( optixTaskGetSerializationKey( task, nullptr, &keySize ) );

        if( keySize > 0 )
        {
            // Task is serializable, try caching
            bool              cacheError = true;
            bool              cacheHit   = false;
            std::vector<char> keyBuffer( keySize );
            OptixResult       gotKey = optixTaskGetSerializationKey( task, keyBuffer.data(), &keySize );
            std::string       cacheKey( keyBuffer.begin(), keyBuffer.end() );

            if( gotKey == OPTIX_SUCCESS )
            {
                std::vector<char> cachedData;
                if( loadFromCache( cacheKey, cachedData ) )
                {
                    // Cache hit - deserialize
                    cacheHit = true;
                    std::vector<OptixTask> additionalTasks( m_maxNumAdditionalTasks );
                    unsigned int           numAdditionalTasksCreated;

                    if( OPTIX_SUCCESS == optixTaskDeserializeOutput( 
                        task, 
                        cachedData.data(), 
                        cachedData.size(), 
                        additionalTasks.data(),
                        m_maxNumAdditionalTasks, 
                        &numAdditionalTasksCreated ) )
                    {
                        cacheError = false;
                        // Add additional tasks to queue for serial processing
                        for( unsigned int i = 0; i < numAdditionalTasksCreated; ++i )
                        {
                            m_taskQueue.push( additionalTasks[i] );
                        }
                    }
                }
            }

            if( cacheError )
            {
                // Give feedback if there was an error while serializing
                if( cacheHit )
                {
                    std::cerr << "[Cache] Error deserializaing output for key: " << cacheKey
                              << ". Treating as cache miss." << std::endl;
                }
                // Cache miss - execute and serialize
                std::cout << "[Cache] miss for key: " << cacheKey << std::endl;
                executeAndCache( task, module, cacheKey );
            }
        }
        else
        {
            // Task is not serializable, execute normally
            executeTaskNormally( task, module );
        }
    }

  private:
    void executeAndCache( OptixTask task, OptixModule module, const std::string& cacheKey )
    {
        // Execute the task
        std::vector<OptixTask> additionalTasks( m_maxNumAdditionalTasks );
        unsigned int           numAdditionalTasksCreated;
        OPTIX_CHECK( optixTaskExecute( task, additionalTasks.data(), m_maxNumAdditionalTasks, &numAdditionalTasksCreated ) );

        // Serialize the output for caching
        bool        cacheError = false;
        size_t      outputSize = 0;
        OptixResult gotSize    = optixTaskSerializeOutput( task, nullptr, &outputSize );

        if( gotSize == OPTIX_SUCCESS && outputSize > 0 )
        {
            cacheError = true;

            // Task is serializable, try caching
            std::vector<char> outputData( outputSize );
            if( OPTIX_SUCCESS == optixTaskSerializeOutput( task, outputData.data(), &outputSize ) )
            {
                cacheError = false;
                saveToCache( cacheKey, outputData.data(), outputSize );
            }
        }

        // Give feedback if there was an error while serializing
        if( cacheError )
        {
            std::cerr << "[Cache] Error serializaing output for key: " << cacheKey << "." << std::endl;
        }

        // Add additional tasks to queue for serial processing
        for( unsigned int i = 0; i < numAdditionalTasksCreated; ++i )
        {
            m_taskQueue.push( additionalTasks[i] );
        }
    }

    void executeTaskNormally( OptixTask task, OptixModule module )
    {
        // Normal execution for non-serializable tasks
        std::vector<OptixTask> additionalTasks( m_maxNumAdditionalTasks );
        unsigned int           numAdditionalTasksCreated;
        OPTIX_CHECK( optixTaskExecute( 
            task, 
            additionalTasks.data(), 
            m_maxNumAdditionalTasks, 
            &numAdditionalTasksCreated ) );

        // Add additional tasks to queue for serial processing
        for( unsigned int i = 0; i < numAdditionalTasksCreated; ++i )
        {
            m_taskQueue.push( additionalTasks[i] );
        }
    }

  public:
    void executeAllTasks( OptixTask initialTask, OptixModule module )
    {
        // Start with the initial task
        m_taskQueue.push( initialTask );

        // Process all tasks serially until queue is empty
        while( !m_taskQueue.empty() )
        {
            OptixTask task = m_taskQueue.front();
            m_taskQueue.pop();
            executeTaskWithCaching( task, module );
        }

        // Get final module state after all tasks complete
        OPTIX_CHECK( optixModuleGetCompilationState( module, &m_moduleState ) );
    }
};

// Global caching task executor
CachingTaskExecutor g_cachingExecutor;


void printUsageAndExit( const char* argv0 )
{
    std::cerr << "Usage  : " << argv0 << " [options]\n";
    std::cerr << "Options: --help | -h                 Print this usage message\n";
    std::cerr << "         --cache-lib <path>          Path to cache shared library (libcache.dll/.so)\n";
    std::cerr << "         --cache-endpoint <endpoint> Cache server endpoint (default: tcp://localhost:5555)\n";
    std::cerr << "         --cache-timeout <ms>        Cache connection timeout in milliseconds (default: 5000)\n";
    exit( 1 );
}


static void context_log_cb( unsigned int level, const char* tag, const char* message, void* /*cbdata */)
{
    std::cerr << "[" << std::setw( 2 ) << level << "][" << std::setw( 12 ) << tag << "]: "
    << message << "\n";
}

int main( int argc, char* argv[] )
{
    std::string cacheLibPath;
    std::string cacheEndpoint = "tcp://localhost:5555";  // Default endpoint
    int         cacheTimeoutMs = 5000;  // Default 5 seconds
    const int   width  = 512;
    const int   height = 384;

    float3 pixelColor = make_float3( 0.462f, 0.725f, 0.f );

    for( int i = 1; i < argc; ++i )
    {
        const std::string arg( argv[i] );
        if( arg == "--help" || arg == "-h" )
        {
            printUsageAndExit( argv[0] );
        }
        else if( arg == "--cache-lib" )
        {
            if( i < argc - 1 )
            {
                cacheLibPath = argv[++i];
            }
            else
            {
                printUsageAndExit( argv[0] );
            }
        }
        else if( arg == "--cache-endpoint" )
        {
            if( i < argc - 1 )
            {
                cacheEndpoint = argv[++i];
            }
            else
            {
                printUsageAndExit( argv[0] );
            }
        }
        else if( arg == "--cache-timeout" )
        {
            if( i < argc - 1 )
            {
                cacheTimeoutMs = std::atoi( argv[++i] );
                if( cacheTimeoutMs <= 0 )
                {
                    std::cerr << "Invalid cache timeout value\n";
                    printUsageAndExit( argv[0] );
                }
            }
            else
            {
                printUsageAndExit( argv[0] );
            }
        }
        else
        {
            std::cerr << "Unknown option '" << arg << "'\n";
            printUsageAndExit( argv[0] );
        }
    }

    try
    {
        if (!cacheLibPath.empty()) {
            std::cout << "Using cachelib " << cacheLibPath << std::endl;
            std::cout << "Using endpoint " << cacheEndpoint << std::endl;
            std::cout << "Using timeout " << cacheTimeoutMs << "ms " << std::endl;
            std::cout << std::endl;
        } else {
            std::cout << "No cachelib provided. Showing simulated cache feedback." << std::endl;
            std::cout << "Disk caching is disabled, and a temporary in-memory cache will be used." << std::endl;
            std::cout << "A working example of a network cache is included." << std::endl;
            std::cout << "See the README for instructions to build & use the cachelib sample library." << std::endl;
            std::cout << std::endl;
        }

        //
        // Initialize CUDA and create OptiX context
        //
        OptixDeviceContext context = nullptr;
        {
            // Initialize CUDA
            CUDA_CHECK( cudaFree( 0 ) );

            CUcontext cuCtx = 0;  // zero means take the current context
            OPTIX_CHECK( optixInit() );
            OptixDeviceContextOptions options = {};
            options.logCallbackFunction       = &context_log_cb;
            options.logCallbackLevel          = 4;
            OPTIX_CHECK( optixDeviceContextCreate( cuCtx, &options, &context ) );
        }

        //
        // Load cache library if specified
        //
        if( !cacheLibPath.empty() )
        {
            // C++11 compatible (upgrade to std::make_unique when moving to C++14)
            g_cacheLib = std::unique_ptr<CacheLibrary>( new CacheLibrary() );
            if( g_cacheLib->loadLibrary( cacheLibPath ) )
            {
                // Set the cache context (using OptiX context as opaque pointer)
                g_cacheContext = static_cast<CacheDeviceContext>( context );

                // Connect to cache service with specified endpoint and timeout
                std::cout << "[Cache] Connecting to cache service at " << cacheEndpoint
                          << " (timeout: " << cacheTimeoutMs << "ms)" << std::endl;
                CacheResult result =
                    connectWithWaitingMessage( g_cacheLib.get(), g_cacheContext, cacheEndpoint.c_str(), cacheTimeoutMs );
                if( result == CACHE_SUCCESS )
                {
                    std::cout << "[Cache] Successfully connected to cache service" << std::endl;
                }
                else
                {
                    std::cerr << "[Cache] Failed to connect to cache service (error code: " << result << ")" << std::endl;
                    std::cerr << "To use the accompanying network cache sample:" << std::endl;
                    std::cerr << "- Use the --cache-lib argument to point to your libcache shared .dll/.so" << std::endl;
                    std::cerr << "- Run the netcache server in a separate shell (python netcache/cache_server.py)" << std::endl;
                    g_cacheLib.reset();
                    g_cacheContext = nullptr;
                }
            }
            else
            {
                g_cacheLib.reset();
            }
        }

        //
        // Compile and execute twice to test cache miss and cache hit
        //
        OptixPipelineCompileOptions pipeline_compile_options = {};
        pipeline_compile_options.usesMotionBlur        = false;
        pipeline_compile_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
        pipeline_compile_options.numPayloadValues      = 2;
        pipeline_compile_options.numAttributeValues    = 2;
        pipeline_compile_options.exceptionFlags        = OPTIX_EXCEPTION_FLAG_NONE;
        pipeline_compile_options.pipelineLaunchParamsVariableName = "params";
        pipeline_compile_options.pipelineLaunchParamsSizeInBytes = sizeof( Params );

        for( int iteration = 0; iteration < 2; ++iteration )
        {
            std::cout << "\n=== Compilation iteration " << (iteration + 1) << " ===" << std::endl;
            
            OptixModule module = nullptr;
            
            //
            // Create module using task API
            //
            {
                OptixModuleCompileOptions module_compile_options = {};
#if OPTIX_DEBUG_DEVICE_CODE
                module_compile_options.optLevel   = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
                module_compile_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
#endif

                size_t      inputSize = 0;
                const char* input = sutil::getInputData( OPTIX_SAMPLE_NAME, OPTIX_SAMPLE_DIR, "optixCustomCache.cu", inputSize );

                // Set maximum number of additional tasks
                g_cachingExecutor.m_maxNumAdditionalTasks = 2;

                OptixTask initialTask;
                OPTIX_CHECK_LOG( optixModuleCreateWithTasks(
                            context,
                            &module_compile_options,
                            &pipeline_compile_options,
                            input,
                            inputSize,
                            LOG, &LOG_SIZE,
                            &module,
                            &initialTask
                            ) );
                
                // Execute all tasks serially with caching
                g_cachingExecutor.executeAllTasks( initialTask, module );
            }

            //
            // Create program groups, including NULL miss and hitgroups
            //
            OptixProgramGroup raygen_prog_group   = nullptr;
            OptixProgramGroup miss_prog_group     = nullptr;
            {
                OptixProgramGroupOptions program_group_options   = {}; // Initialize to zeros

                OptixProgramGroupDesc raygen_prog_group_desc  = {};
                raygen_prog_group_desc.kind                     = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
                raygen_prog_group_desc.raygen.module            = module;
                raygen_prog_group_desc.raygen.entryFunctionName = "__raygen__optix_custom_cache";
                OPTIX_CHECK_LOG( optixProgramGroupCreate(
                            context,
                            &raygen_prog_group_desc,
                            1,   // num program groups
                            &program_group_options,
                            LOG, &LOG_SIZE,
                            &raygen_prog_group
                            ) );

                // Leave miss group's module and entryfunc name null
                OptixProgramGroupDesc miss_prog_group_desc = {};
                miss_prog_group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
                OPTIX_CHECK_LOG( optixProgramGroupCreate(
                            context,
                            &miss_prog_group_desc,
                            1,   // num program groups
                            &program_group_options,
                            LOG, &LOG_SIZE,
                            &miss_prog_group
                            ) );
            }

            //
            // Link pipeline
            //
            OptixPipeline pipeline = nullptr;
            {
                OptixProgramGroup program_groups[] = { raygen_prog_group };

                OptixPipelineLinkOptions pipeline_link_options = {};
                pipeline_link_options.maxTraceDepth            = 0;
                // relying on internal default implementation to compute pipeline stack size
                OPTIX_CHECK_LOG( optixPipelineCreate(
                            context,
                            &pipeline_compile_options,
                            &pipeline_link_options,
                            program_groups,
                            sizeof( program_groups ) / sizeof( program_groups[0] ),
                            LOG, &LOG_SIZE,
                            &pipeline
                            ) );
            }

            //
            // Set up shader binding table
            //
            OptixShaderBindingTable sbt = {};
            {
                CUdeviceptr  raygen_record;
                const size_t raygen_record_size = sizeof( RayGenSbtRecord );
                CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &raygen_record ), raygen_record_size ) );
                RayGenSbtRecord rg_sbt;
                OPTIX_CHECK( optixSbtRecordPackHeader( raygen_prog_group, &rg_sbt ) );
                rg_sbt.data = { pixelColor.x, pixelColor.y, pixelColor.z };
                CUDA_CHECK( cudaMemcpy(
                            reinterpret_cast<void*>( raygen_record ),
                            &rg_sbt,
                            raygen_record_size,
                            cudaMemcpyHostToDevice
                            ) );

                CUdeviceptr miss_record;
                size_t      miss_record_size = sizeof( MissSbtRecord );
                CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &miss_record ), miss_record_size ) );
                MissSbtRecord ms_sbt;
                OPTIX_CHECK( optixSbtRecordPackHeader( miss_prog_group, &ms_sbt ) );
                CUDA_CHECK( cudaMemcpy(
                            reinterpret_cast<void*>( miss_record ),
                            &ms_sbt,
                            miss_record_size,
                            cudaMemcpyHostToDevice
                            ) );

                sbt.raygenRecord                = raygen_record;
                sbt.missRecordBase              = miss_record;
                sbt.missRecordStrideInBytes     = sizeof( MissSbtRecord );
                sbt.missRecordCount             = 1;
            }

            sutil::CUDAOutputBuffer<uchar4> output_buffer( sutil::CUDAOutputBufferType::CUDA_DEVICE, width, height );

            //
            // launch
            //
            {
                CUstream stream;
                CUDA_CHECK( cudaStreamCreate( &stream ) );

                Params params;
                params.image       = output_buffer.map();
                params.image_width = width;

                // zero-initialize our image (so we can check below if the pixels were written & contain the expected value)
                cudaMemset(params.image, 0, sizeof(uchar4) * width * height);

                CUdeviceptr d_param;
                CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_param ), sizeof( Params ) ) );
                CUDA_CHECK( cudaMemcpy(
                            reinterpret_cast<void*>( d_param ),
                            &params, sizeof( params ),
                            cudaMemcpyHostToDevice
                            ) );

                OPTIX_CHECK( optixLaunch( pipeline, stream, d_param, sizeof( Params ), &sbt, width, height, /*depth=*/1 ) );
                CUDA_SYNC_CHECK();

                output_buffer.unmap();
                CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_param ) ) );
            }

            //
            // Verify the launch results just for good measure
            //
            {
                const uchar4* buffer_data = output_buffer.getHostPointer();
                
                // Expected color from raygen (pixelColor) converted to uchar4
                const unsigned char expected_r = static_cast<unsigned char>( pixelColor.x * 255.0f );
                const unsigned char expected_g = static_cast<unsigned char>( pixelColor.y * 255.0f );
                const unsigned char expected_b = static_cast<unsigned char>( pixelColor.z * 255.0f );
                
                bool success = true;
                for( int i = 0; i < width * height; ++i )
                {
                    if( buffer_data[i].x != expected_r || 
                        buffer_data[i].y != expected_g || 
                        buffer_data[i].z != expected_b )
                    {
                        std::cerr << "pixel check FAILED: Pixel " << i << " has color (" 
                                  << (int)buffer_data[i].x << ", " 
                                  << (int)buffer_data[i].y << ", " 
                                  << (int)buffer_data[i].z << ") but expected (" 
                                  << (int)expected_r << ", " 
                                  << (int)expected_g << ", " 
                                  << (int)expected_b << ")\n";
                        success = false;
                        break;
                    }
                }
                
                if( success )
                {
                    std::cout << "pixel check PASSED: All pixels have the expected color\n";
                }
            }

            //
            // Cleanup iteration resources
            //
            {
                CUDA_CHECK( cudaFree( reinterpret_cast<void*>( sbt.raygenRecord       ) ) );
                CUDA_CHECK( cudaFree( reinterpret_cast<void*>( sbt.missRecordBase     ) ) );

                OPTIX_CHECK( optixPipelineDestroy( pipeline ) );
                OPTIX_CHECK( optixProgramGroupDestroy( miss_prog_group ) );
                OPTIX_CHECK( optixProgramGroupDestroy( raygen_prog_group ) );
                OPTIX_CHECK( optixModuleDestroy( module ) );
            }
        } // End of compilation iteration loop

        //
        // Final cleanup
        //
        {
            // Disconnect from cache service if connected
            if (g_cacheLib && g_cacheLib->handle) {
                g_cacheLib->disconnect(g_cacheContext);
                std::cout << "Disconnected from cache service" << std::endl;
            }
            g_cacheLib.reset();
            g_cacheContext = nullptr;

            OPTIX_CHECK( optixDeviceContextDestroy( context ) );
        }
    }
    catch( std::exception& e )
    {
        std::cerr << "Caught exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
