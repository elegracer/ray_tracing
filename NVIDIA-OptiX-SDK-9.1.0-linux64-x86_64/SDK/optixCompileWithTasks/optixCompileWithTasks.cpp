/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <optix.h>
#include <optix_function_table_definition.h>
#include <optix_stack_size.h>
#include <optix_stubs.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <cuda.h>
#include <cuda_runtime.h>

#include <CompileWithTasks.h>
#include <sutil/Exception.h>

using namespace optix::CompileWithTasks;



static void context_log_cb( unsigned int level, const char* tag, const char* message, void* /*cbdata */)
{
    std::cerr << "[" << std::setw( 2 ) << level << "][" << std::setw( 12 ) << tag << "]: "
    << message << "\n";
}

OptixDeviceContext          s_context                = 0;
OptixDeviceContextOptions   s_options                = {};
OptixModuleCompileOptions   s_moduleCompileOptions   = {};
OptixPipelineCompileOptions s_pipelineCompileOptions = {};
unsigned int                s_defaultLogLevel        = 4;

OptixTaskExecutePool        g_pool;


static void SetUp()
{
    CUDA_CHECK( cudaFree( 0 ) );
    void* handle;
    OPTIX_CHECK( optixInitWithHandle( &handle ) );

    s_options.logCallbackFunction = &context_log_cb;
    s_options.logCallbackLevel    = s_defaultLogLevel;
    CUcontext cuCtx               = 0;  // zero means take the current context
    OPTIX_CHECK( optixDeviceContextCreate( cuCtx, &s_options, &s_context ) );
}

static void SetLoggingLevel( unsigned int level )
{
    OPTIX_CHECK( optixDeviceContextSetLogCallback( s_context, &context_log_cb, 0, level ) );
}

static void TearDown()
{
    OPTIX_CHECK( optixDeviceContextDestroy( s_context ) );
}

std::string readInputFile( const std::string& filename )
{
    std::ifstream input( filename.c_str(), std::ios::binary );
    if( !input )
    {
        std::cerr << "ERROR: Failed to open input file '" << filename << "'\n";
        exit( 1 );
    }

    std::vector<unsigned char> buffer( std::istreambuf_iterator<char>( input ), {} );
    return std::string( buffer.begin(), buffer.end() );
}

struct Timer
{
    Timer() { m_start = m_clock.now(); }

    double elapsed() const
    {
        std::chrono::duration<double> e = m_clock.now() - m_start;
        return e.count();
    }

    friend std::ostream& operator<<( std::ostream& out, const Timer& timer ) { return out << timer.elapsed(); }
    std::chrono::high_resolution_clock             m_clock;
    std::chrono::high_resolution_clock::time_point m_start;
};

void compileModules( const std::vector<std::string>& input, int numIters = 1 )
{
    std::vector<OptixModule> modules( input.size() );
    Timer                    overallTimer;
    for( int i = 0; i < numIters; ++i )
    {
        Timer iterTimer;
        for( size_t idx = 0; idx < input.size(); ++idx )
        {
            Timer iterTimer;
            OPTIX_CHECK( optixModuleCreate( s_context, &s_moduleCompileOptions, &s_pipelineCompileOptions,
                                            input[idx].c_str(), input[idx].size(), 0, 0, &modules[idx] ) );
        }
        if( i == 0 )
        {
            SetLoggingLevel( 0 );
        }
        std::cout << "iter[" << i << "] duration = " << iterTimer << " seconds\n";
    }
    double seconds = overallTimer.elapsed();
    SetLoggingLevel( s_defaultLogLevel );
    std::cout << "over all time " << seconds << " seconds, per iter average = " << seconds / numIters << "\n";
    std::cout << "Successfully compiled\n";
}

void compileModulesWithTasks( const std::vector<std::string>& input, int numIters = 1 )
{
    std::vector<OptixModule>             modules( input.size() );
    std::vector<OptixModuleCompileState> states( input.size() );
    Timer                    overallTimer;
    for( int i = 0; i < numIters; ++i )
    {
        Timer iterTimer;
        for( size_t idx = 0; idx < input.size(); ++idx )
        {
            OptixTask initialTask;
            OPTIX_CHECK( optixModuleCreateWithTasks( s_context, &s_moduleCompileOptions, &s_pipelineCompileOptions,
                                                     input[idx].c_str(), input[idx].size(), 0, 0, &modules[idx], &initialTask ) );
            OPTIX_CHECK( optixModuleGetCompilationState( modules[idx], &states[idx] ) );
            g_pool.addTaskAndExecute( initialTask, modules[idx], states[idx] );
        }
        OPTIX_CHECK( g_pool.waitForModuleTasks( states.data(), states.size() ) );
        if( i == 0 )
        {
            SetLoggingLevel( 0 );
        }

        std::cout << "iter[" << i << "] duration = " << iterTimer << " seconds\n";
    }
    double seconds = overallTimer.elapsed();
    SetLoggingLevel( s_defaultLogLevel );
    std::cout << "over all time " << seconds << " seconds, per iter average = " << seconds / numIters << "\n";
    std::cout << "Successfully compiled\n";
}

void printUsageAndExit( const std::string& argv0, bool doExit = true )
{
    // These provide a rudimentary set of options and are by no means exhaustive to the
    // set of compile options available to optixModuleCreate.
    std::cerr << "\nUsage  : " << argv0 << " [options] <input_file>\n"
              << "App options:\n"
              << "  -h   | --help                     Print this usage message\n"
              << "  -na  | --num-attributes <N>       Number of attribute values (up to 8, default 2)\n"
              << "  -npv | --num-payload-values <N>   Number of payload values (up to "
              << OPTIX_COMPILE_DEFAULT_MAX_PAYLOAD_VALUE_COUNT << ", default 2)\n"
              << "  -npt | --num-payload-types <N>    Number of payload types (up to "
              << OPTIX_COMPILE_DEFAULT_MAX_PAYLOAD_TYPE_COUNT << ", default 1)\n"
              << "  -ni  | --num-iters <N>            Number of iterations to compile. > 1 disables disk cache (default 1)\n"
              << "  -dt  | --disable-tasks            Disable compilation with tasks (default enabled)\n"
              << "  -dc  | --disable-cache            Disable caching of compiled ptx on disk (default enabled)\n"
              << "  -nt  | --num-threads <N>          Number of threads (default 1)\n"
              << "  -mt  | --max-num-tasks <N>        Maximum number of additional tasks (default 2)\n"
              << "       | --filenames <list>         A quote-enclosed, semicolon delimited list of PTX input files\n"
              << "  -ue  | --user-exceptions          Enable user exceptions (OPTIX_EXCEPTION_FLAG_USER)\n"
              << "  -g                                Enable debug support (implies -O0)\n"
        << std::endl;

    if( doExit )
        exit( 1 );
}

int main( int argc, char** argv )
{
    bool  useTasks    = true;
    bool  useCache    = true;
    int   numThreads  = 2;
    int   maxNumTasks = 2;
    int   numIters    = 1;
    std::vector<std::string>      filenames;
    std::vector<OptixPayloadType> types;
    std::vector<unsigned int>     defaultPayloadSemantics;

    if( argc < 2 )
    {
        std::cerr << "\nERROR: No input file provided for compilation\n";
        printUsageAndExit( argv[0] );
    }

    for( int i = 1; i < argc; ++i )
    {
        std::string arg( argv[i] );
        if( arg == "-h" || arg == "--help" )
        {
            printUsageAndExit( argv[0] );
        }
        else if( arg == "-na" || arg == "--num-attributes" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            s_pipelineCompileOptions.numAttributeValues = atoi( argv[++i] );
        }
        else if( arg == "-npv" || arg == "--num-payload-values" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            s_pipelineCompileOptions.numPayloadValues = atoi( argv[++i] );
        }
        else if( arg == "-npt" || arg == "--num-payload-types" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            int numTypes = atoi( argv[++i] );
            types.resize( numTypes, {} );
            defaultPayloadSemantics.resize( s_pipelineCompileOptions.numPayloadValues, 0 );
            for( unsigned int& payloadSemantic : defaultPayloadSemantics )
            {
                payloadSemantic = OPTIX_PAYLOAD_SEMANTICS_TRACE_CALLER_READ_WRITE
                                  | OPTIX_PAYLOAD_SEMANTICS_CH_READ_WRITE | OPTIX_PAYLOAD_SEMANTICS_MS_READ_WRITE
                                  | OPTIX_PAYLOAD_SEMANTICS_AH_READ_WRITE | OPTIX_PAYLOAD_SEMANTICS_IS_READ_WRITE;
            }
            for( OptixPayloadType& type : types )
            {
                type.numPayloadValues = static_cast<unsigned int>( defaultPayloadSemantics.size() );
                type.payloadSemantics = defaultPayloadSemantics.data();
            }
            s_pipelineCompileOptions.numPayloadValues = 0;
            s_moduleCompileOptions.numPayloadTypes    = numTypes;
            s_moduleCompileOptions.payloadTypes       = types.data();
        }
        else if( arg == "-ni" || arg == "--num-iters" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            numIters = atoi( argv[++i] );
        }
        else if( arg == "-dt" || arg == "--disable-tasks" )
        {
            useTasks = false;
        }
        else if( arg == "-dc" || arg == "--disable-cache" )
        {
            useCache = false;
        }
        else if( arg == "-nt" || arg == "--num-threads" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            numThreads = atoi( argv[++i] );
        }
        else if( arg == "-mt" || arg == "--max-num-tasks" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            maxNumTasks = atoi( argv[++i] );
        }
        else if( arg == "-ue" || arg == "--user-exceptions" )
        {
            s_pipelineCompileOptions.exceptionFlags = OPTIX_EXCEPTION_FLAG_USER;
        }
        else if( arg == "-g" )
        {
            s_moduleCompileOptions.optLevel   = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
            s_moduleCompileOptions.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
        }
        else if( arg == "--filenames" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );

            // Tokenize the filename list into a vector of std::strings
            std::string       namesArg = argv[++i];
            std::stringstream namesStream( namesArg );
            std::string       filename;
            while( std::getline( namesStream, filename, ';' ) )
                filenames.push_back( filename );
        }
        else
        {
            filenames.push_back( arg );
        }
    }

    SetUp();
    if( numIters > 1 || !useCache )
        optixDeviceContextSetCacheEnabled( s_context, 0 );

    std::vector<std::string> input;
    for( const std::string& filename : filenames )
    {
        std::string ptx = readInputFile( filename );
        input.push_back( ptx );
    }

    if( useTasks )
    {
        std::cout << "Running with " << numThreads << " threads and " << maxNumTasks << " maximum number of tasks\n";
        g_pool.m_threadPool.startPool( numThreads );
        g_pool.m_maxNumAdditionalTasks = maxNumTasks;
        compileModulesWithTasks( input, numIters );
        g_pool.m_threadPool.terminate();
    }
    else
    {
        compileModules( input, numIters );
    }

    TearDown();

    return 0;
}
