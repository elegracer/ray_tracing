/*
 * SPDX-FileCopyrightText: Copyright (c) 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <glad/glad.h>

#include <optix_function_table_definition.h>
#include <optix_stubs.h>

#include <sutil/CUDAOutputBuffer.h>
#include <sutil/GLDisplay.h>

#include <GLFW/glfw3.h>
#include <imgui/imgui.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Compilation constants
static const unsigned int MAX_NUM_ADDITIONAL_TASKS = 100;

// UI constants
static const float PROGRESS_BAR_HEIGHT_SCALE = 4.0f;
static const float PROGRESS_BAR_WIDTH_SCALE  = 0.8f;

enum class CompileState
{
    IDLE              = 0,
    COMPILE_REQUESTED = 1,
    COMPILE_STARTING  = 2,
    COMPILING         = 3,
    CANCEL_REQUESTED  = 4,
    CANCELING         = 5,
    CANCELED          = 6
};

OptixDeviceContext                    g_context{};
OptixModule                           g_module{};
std::mutex                            g_moduleMutex;
std::chrono::steady_clock             g_clock;
std::chrono::steady_clock::time_point g_start;
std::atomic<float>                    g_averageTime{ 0.0f };
int                                   g_iterations{ 0 };
CompileState                          g_compileState{ CompileState::COMPILE_STARTING };

static void logCallback( unsigned int level, const char* tag, const char* message, void* /*cbdata*/ )
{
    std::cerr << "[" << std::setw( 2 ) << level << "][" << std::setw( 12 ) << tag << "]: " << message << "\n";
}

static void setUp()
{
    CUDA_CHECK( cudaFree( 0 ) );

    OPTIX_CHECK( optixInit() );

    OptixDeviceContextOptions options{};
    options.logCallbackFunction = &logCallback;
    options.logCallbackLevel    = 0;
    OPTIX_CHECK( optixDeviceContextCreate( 0, &options, &g_context ) );
    OPTIX_CHECK( optixDeviceContextSetCacheEnabled( g_context, 0 ) );
}

static void tearDown()
{
    OPTIX_CHECK( optixDeviceContextDestroy( g_context ) );
}

static long long unsigned int getElapsedMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>( g_clock.now() - g_start ).count();
}

static void compileModule( std::string& input, bool loop )
{
    // Obtain lock until module is created
    std::unique_lock<std::mutex> moduleLock( g_moduleMutex );

    OptixTask                   task;
    OptixPipelineCompileOptions pipelineCompileOptions{};
    OptixModuleCompileOptions   moduleCompileOptions{};

    std::queue<OptixTask>  taskQueue;
    std::vector<OptixTask> additionalTasks( MAX_NUM_ADDITIONAL_TASKS );
    unsigned int           numAdditionalTasks;

    g_iterations          = 0;
    float       totalTime = 0;
    OptixResult result    = OPTIX_SUCCESS;
    g_compileState        = CompileState::COMPILING;

    // Loop compilation of the module until the user cancels
    do
    {
        g_start = g_clock.now();
        OPTIX_CHECK( optixModuleCreateWithTasks( g_context, &moduleCompileOptions, &pipelineCompileOptions,
                                                 input.c_str(), input.size(), nullptr, nullptr, &g_module, &task ) );

        // This signals to the main thread that the module handle is now available for cancelation
        moduleLock.unlock();

        // Compile the module
        taskQueue.push( task );
        while( !taskQueue.empty() )
        {
            task = taskQueue.front();
            taskQueue.pop();

            result = optixTaskExecute( task, additionalTasks.data(), MAX_NUM_ADDITIONAL_TASKS, &numAdditionalTasks );

            // If compilation completed, failed, or was canceled, there will be no additional tasks, and the loop will end
            for( unsigned int i = 0; i < numAdditionalTasks; ++i )
            {
                taskQueue.push( additionalTasks[i] );
            }
        }
        unsigned long long int iterationTime = getElapsedMs();
        g_iterations++;

        // Module handle will no longer be a valid cancelation target after the destroy call
        moduleLock.lock();
        OPTIX_CHECK( optixModuleDestroy( g_module ) );

        if( result == OPTIX_SUCCESS )
        {
            // Successful compile, update average time
            totalTime += iterationTime;
            g_averageTime = totalTime / g_iterations;
        }
        else if( result == OPTIX_ERROR_CREATION_CANCELED )
        {
            g_compileState = CompileState::CANCELED;
        }
        else
        {
            // Failed compile (not canceled), print error and exit loop
            std::cerr << "Compilation failed with '" << result << "'\n";
            g_compileState = CompileState::IDLE;
        }
    } while( loop && result == OPTIX_SUCCESS );
}

static void printUsageAndExit( const std::string& argv0 )
{
    std::cerr << "Usage : " << argv0 << " [options] <input_file>\n"
              << "App options:\n"
              << "  -h | --help                             Print this usage message\n"
              << "  -f | --file                        <F>  File containing optix-ir or ptx to compile "
              << "(default=optixCompileCancel.cu)\n"
              << "  -p | --percentCompiledBeforeCancel <P>  "
              << "Cancel compilation after P% (between 1 and 99) and print stats to stdout\n"
              << std::endl;

    exit( 1 );
}

static void loadFile( std::string& filename, std::string& inputStr )
{
    // Load the input ptx/optix-ir
    std::ifstream input( filename.c_str(), std::ios::binary );
    if( !input )
    {
        std::cerr << "ERROR: Failed to open input file '" << filename << "'\n";
        exit( 1 );
    }
    std::vector<unsigned char> buffer( std::istreambuf_iterator<char>( input ), {} );
    inputStr = std::string( buffer.begin(), buffer.end() );
}

static void keyCallback( GLFWwindow* window, int32_t key, int32_t /*scancode*/, int32_t action, int32_t /*mods*/ )
{
    if( action == GLFW_PRESS )
    {
        if( key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE )
        {
            glfwSetWindowShouldClose( window, true );
        }
        else if( key == GLFW_KEY_C )
        {
            if( g_compileState == CompileState::COMPILING )
            {
                g_compileState = CompileState::CANCEL_REQUESTED;
            }
            else if( g_compileState == CompileState::IDLE )
            {
                g_compileState = CompileState::COMPILE_REQUESTED;
            }
        }
    }
}

static std::string formatCancelStats( long long unsigned int compileDuration, long long unsigned int cancelDuration )
{
    return "Canceled on compile " + std::to_string( g_iterations ) + "\nCancel requested after "
           + std::to_string( compileDuration ) + " ms\nCancel finished after " + std::to_string( compileDuration + cancelDuration )
           + " ms\nCancel operation took " + std::to_string( cancelDuration ) + " ms\n\n";
}

static void displayStats( const char* text, float x, float y, float compileProgress, float cancelProgress )
{
    // Calculate dimensions
    ImVec2 textSize          = ImGui::CalcTextSize( text );
    float  progressBarHeight = ImGui::GetFrameHeight() * PROGRESS_BAR_HEIGHT_SCALE;

    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize( glfwGetCurrentContext(), &framebufferWidth, &framebufferHeight );
    float progressBarWidth = framebufferWidth * PROGRESS_BAR_WIDTH_SCALE;

    float uiWindowHeight = textSize.y + y + progressBarHeight + ImGui::GetStyle().ItemSpacing.y;
    float uiWindowWidth  = progressBarWidth + x * 2.0f;

    // Setup overlay window
    ImGui::SetNextWindowSize( ImVec2( uiWindowWidth, uiWindowHeight ) );
    ImGui::SetNextWindowBgAlpha( 0.0f );
    ImGui::SetNextWindowPos( ImVec2( x, y ) );
    ImGui::Begin( "TextOverlayFG", nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                      | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs );

    // Draw progress bar
    ImVec2 progressBarPos = ImGui::GetCursorScreenPos();
    ImGui::ProgressBar( compileProgress, ImVec2( progressBarWidth, progressBarHeight ), "" );

    // Overlay cancel duration in red (start from current progress position to avoid gaps)
    if( cancelProgress > 0.0f )
    {
        float  cancelStartX = std::round( progressBarPos.x + compileProgress * progressBarWidth );
        float  cancelWidth  = std::round( cancelProgress * progressBarWidth );
        ImVec2 rectMin( cancelStartX, progressBarPos.y );
        ImVec2 rectMax( cancelStartX + cancelWidth, progressBarPos.y + progressBarHeight );
        ImGui::GetWindowDrawList()->AddRectFilled( rectMin, rectMax, ImColor( 1.0f, 0.0f, 0.0f, 1.0f ) );
    }

    // Draw text underneath
    ImGui::TextColored( ImColor( 0.7f, 0.7f, 0.7f, 1.0f ), "%s", text );

    ImGui::End();
}


int main( int argc, char** argv )
{
    std::string  input          = "";
    unsigned int compilePercent = 0;

    for( int i = 1; i < argc; ++i )
    {
        std::string arg( argv[i] );
        if( arg == "-h" || arg == "--help" )
        {
            printUsageAndExit( argv[0] );
        }
        else if( arg == "-f" || arg == "--file" )
        {
            if( i >= argc - 1 )
                printUsageAndExit( argv[0] );
            std::string filename = argv[++i];
            loadFile( filename, input );
        }
        else if( arg == "-p" || arg == "--percentCompiledBeforeCancel" )
        {
            if( i >= argc - 1 )
                printUsageAndExit( argv[0] );
            compilePercent = atoi( argv[++i] );
            if( compilePercent < 1 || compilePercent > 99 )
                printUsageAndExit( argv[0] );
        }
    }

    if( input.empty() )
    {
        std::cerr << "No input file provided, using default optixCompileCancel.cu\n";

        size_t inputSize = 0;
        const char* inputData = sutil::getInputData( OPTIX_SAMPLE_NAME, OPTIX_SAMPLE_DIR, "optixCompileCancel.cu", inputSize );
        input = std::string( inputData, inputSize );
    }
    setUp();

    // Get a baseline compile time
    std::cout << "\nCompiling once to find the initial estimated duration... ";
    compileModule( input, false );
    std::cout << "done\n";

    if( !compilePercent )
    {
        int         xRes   = 0;
        int         yRes   = 0;
        GLFWwindow* window = sutil::initUI( "optixCompileCancel", 512, 512 );
        glfwSetKeyCallback( window, keyCallback );

        while( !glfwWindowShouldClose( window ) )
        {
            // Kick off compile thread
            std::thread worker( compileModule, std::ref( input ), true );

            sutil::GLDisplay                glDisplay;
            sutil::CUDAOutputBuffer<uchar4> outputBuffer( sutil::CUDAOutputBufferType::GL_INTEROP, 512, 512 );

            float                  averageDuration = g_averageTime.load();
            long long unsigned int compileDuration = 0;
            long long unsigned int cancelDuration  = 0;

            std::string compileText = "Average " + std::to_string( averageDuration ) + " ms/compile\n\n";
            std::string cancelText  = "Press 'c' to cancel";

            // Wait for cancel request from user
            bool restart = false;
            while( !glfwWindowShouldClose( window ) && !restart )
            {
                glfwPollEvents();

                switch( g_compileState )
                {
                    case CompileState::COMPILE_REQUESTED:
                        restart        = true;
                        g_compileState = CompileState::COMPILE_STARTING;
                        break;
                    case CompileState::COMPILING: {
                        // Lock to prevent access to g_start while it is being modified
                        std::lock_guard<std::mutex> moduleLock( g_moduleMutex );
                        compileDuration = getElapsedMs();
                    }
                        averageDuration = g_averageTime.load();
                        compileText     = "Average " + std::to_string( averageDuration ) + " ms/compile\n\n";
                        break;
                    case CompileState::CANCEL_REQUESTED: {
                        // Make sure the compiler thread can't destroy the module while the main thread is trying to cancel
                        std::lock_guard<std::mutex> moduleLock( g_moduleMutex );

                        // Issue non-blocking cancel (no flags = non-blocking)
                        OPTIX_CHECK( optixModuleCancelCreation( g_module, static_cast<OptixCreationFlags>( 0 ) ) );
                    }
                        g_compileState = CompileState::CANCELING;
                        // Fall through to CANCELING to start calculating cancelDuration
                    case CompileState::CANCELING:
                        // No need to acquire lock for getElapsedMs since we are
                        // canceling and m_start is guaranteed not to change again
                        cancelDuration = getElapsedMs() - compileDuration;
                        break;
                    case CompileState::CANCELED:
                        worker.join();
                        cancelText = formatCancelStats( compileDuration, cancelDuration );
                        cancelText += "Press 'c' to compile\n";
                        g_compileState = CompileState::IDLE;
                        break;
                    default:
                        break;
                }

                // Display window
                glfwGetFramebufferSize( window, &xRes, &yRes );
                glDisplay.display( static_cast<int>( outputBuffer.width() ), static_cast<int>( outputBuffer.height() ),
                                   xRes, yRes, outputBuffer.getPBO() );

                sutil::beginFrameImGui();
                displayStats( ( compileText + cancelText ).c_str(), 20.0f, 20.0f, compileDuration / averageDuration, cancelDuration / averageDuration );
                sutil::endFrameImGui();

                glfwSwapBuffers( window );
            }

            // Kill compiler thread
            if( worker.joinable() )
            {
                std::unique_lock<std::mutex> moduleLock( g_moduleMutex );
                OPTIX_CHECK( optixModuleCancelCreation( g_module, OPTIX_CREATION_FLAG_BLOCK_UNTIL_EFFECTIVE ) );

                moduleLock.unlock();
                worker.join();
            }
        }
        sutil::cleanupUI( window );
    }
    else
    {
        // Non-interactive mode: benchmark compile time then cancel at specified percentage
        float average      = g_averageTime.load();
        int   cancelWaitMs = static_cast<int>( ( compilePercent / 100.0f ) * average );
        std::cout << "Compile takes roughly " << average << " ms.\n";
        std::cout << "\nRecompiling and attempting to cancel after " << cancelWaitMs << " ms (" << compilePercent << "%)...\n\n";

        // Kick off compile thread
        std::thread worker( compileModule, std::ref( input ), true );

        // Wait, then cancel
        std::this_thread::sleep_for( std::chrono::milliseconds( cancelWaitMs ) );

        // Make sure the compiler thread can't destroy the module while the main thread is trying to cancel
        std::unique_lock<std::mutex> moduleLock( g_moduleMutex );

        long long unsigned int cancelStart = getElapsedMs();
        OPTIX_CHECK( optixModuleCancelCreation( g_module, OPTIX_CREATION_FLAG_BLOCK_UNTIL_EFFECTIVE ) );
        long long unsigned int cancelEnd = getElapsedMs();

        moduleLock.unlock();
        worker.join();

        std::cout << formatCancelStats( cancelStart, cancelEnd - cancelStart );
    }
    tearDown();

    return 0;
}
