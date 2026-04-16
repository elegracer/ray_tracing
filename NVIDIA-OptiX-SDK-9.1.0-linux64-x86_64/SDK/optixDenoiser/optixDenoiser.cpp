/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "OptiXDenoiser.h"
#include "applyflow.h"

#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

#include <stdlib.h>
#include <assert.h>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>


//------------------------------------------------------------------------------
//
//  optixDenoiser -- Demonstration of the OptiX denoising API.
//
//------------------------------------------------------------------------------

void printUsageAndExit( const std::string& argv0 )
{
    std::cout << "Usage  : " << argv0 << " [options] {-A | --AOV aov.exr} color.exr\n"
              << "Options: -n | --normal <normal.exr | layer name>\n"
              << "         -a | --albedo <albedo.exr | layer name>\n"
              << "         -b | --beauty <layer name>\n"
              << "         -f | --flow   <flow.exr | layer name>\n"
              << "         -A | --AOV    <aov.exr | layer name>\n"
              << "         -S            <specular aov.exr | layer name>\n"
              << "         -T            <flowTrustworthiness.exr | layer name>\n"
              << "         -o | --out    <out.exr>\n"
              << "         -F | --Frames <int int> first and last frame number in sequence\n"
              << "         -e | --exposure <float> apply exposure on output images\n"
              << "         -t | --tilesize <int int> use tiling to save GPU memory\n"
              << "         -alpha denoise alpha channel\n"
              << "         -fmul <x y> multiply flow vector components\n"
              << "         -fp32 write images with 32-bit precision, default 16 bit\n"
              << "         -up2 upscale image by factor of 2\n"
              << "         -z apply flow to input images (no denoising), for flow vector verification\n"
              << "in sequences, first occurrence of '+' characters substring in filenames is replaced by framenumber\n"
              << "instead of EXR filenames layer names inside the given EXR file <color.exr> could be specified\n"
              << std::endl;
    exit( 0 );
}

static double getCurrentTime()
{
    return std::chrono::duration_cast< std::chrono::duration< double > >
        ( std::chrono::high_resolution_clock::now().time_since_epoch() ).count();
}

static void context_log_cb( uint32_t level, const char* tag, const char* message, void* /*cbdata*/ )
{
    if( level < 4 )
    {
        std::cerr << "[" << std::setw( 2 ) << level << "][" << std::setw( 12 ) << tag << "]: "
                  << message << "\n";
    }
}

// Layers given on the command line (-a, -n, -f, -b) are passed as 'layerName'. If 'layerName' is 0,
// layers are not searched and RGB is assumed.
// An input filename (possibly containing all layers) on the command line is passed as 'inputFileName'.
// If the inputFileName has a layer with the name 'layerName', it is loaded from 'inputFileName',
// otherwise it is assumed that 'layerName' is the filename to load from (layer name RGB).
// Note that we use a modified tinyexr.h file which has an alternative component search for XYZ,
// as this is used for normals and velocity. The original version searches only for RGB.

static OptixImage2D loadImageLayer( const char* inputFileName, const char* layerName, EXRImage* cachedImage )
{
    const char* err = nullptr;
    bool layerExists = false;

    std::string baseErrorMessage = std::string( "failed to load from file \"" ) + std::string( inputFileName ) + "\" ";

    if( layerName && std::string( layerName ).find( "exr" ) == std::string::npos )
    {
        const char** layerNames = nullptr;
        int numLayers = 0;

        int32_t res = EXRLayers( inputFileName, &layerNames, &numLayers, &err);
        if( res != TINYEXR_SUCCESS )
        {
            std::string emsg = baseErrorMessage;
            if( err )
            {
		emsg += err;
                FreeEXRErrorMessage( err );
            }
	    fprintf( stderr, "Error loading EXR file (%s): %s\n", inputFileName, emsg.c_str() );
	    exit( 1 );
        }
        for( int i=0; i < numLayers; i++ )
        {
            if( std::string( layerNames[i] ) == std::string( layerName ) )
                layerExists = true;
        }
        if( !layerExists )
        {
            std::string emsg = baseErrorMessage + std::string( ", layer \"" ) + std::string( layerName ) + std::string( "\" not found.\n" );
	    fprintf( stderr, "Error loading EXR file (%s): %s\n", inputFileName, emsg.c_str() );
	    exit( 1 );
        }
    }

    float*  data = nullptr;
    int32_t res, w, h;
    if( layerExists )
        res = LoadEXRWithLayer( &data, &w, &h, inputFileName, layerName, &err, cachedImage );
    else	// load RGB from input file (if layerName is given, it must be an EXR file to load).
                // if layerName is null, it must be beauty, so load RGB from beauty EXR
        res = LoadEXR( &data, &w, &h, inputFileName, &err );

    if( res != TINYEXR_SUCCESS)
    {
        std::string emsg = baseErrorMessage;
        if ( err )
        {
            emsg += err;
            FreeEXRErrorMessage( err );
        }
        fprintf( stderr, "Error loading EXR file(%s): %s\n", inputFileName, emsg.c_str() );
        exit( 1 );
    }

    OptixImage2D image = createOptixImage2D( w, h, OPTIX_PIXEL_FORMAT_FLOAT4 );
    cudaMemcpy( (void*)image.data, (void*)data, sizeof(float) * 4 * w * h, cudaMemcpyHostToDevice );

    free( data );

    return image;
}

// save image to EXR file

static void saveImageEXR( const char* fname, const OptixImage2D& image, bool fp16 )
{
    const std::string filename( fname );

    std::vector<char> data( image.rowStrideInBytes * image.height );

    cudaMemcpy( &data[0], (void*)image.data, image.rowStrideInBytes * image.height, cudaMemcpyDeviceToHost );

    switch( image.format )
    {
        case OPTIX_PIXEL_FORMAT_FLOAT3:
        {
            const char* err;
            int32_t ret = SaveEXR(
                    reinterpret_cast<float*>( &data[0] ),
                    image.width,
                    image.height,
                    3, // num components
                    static_cast<int32_t>( fp16 ), // save_as_fp16
                    filename.c_str(),
                    &err );

            if( ret != TINYEXR_SUCCESS )
            {
                fprintf( stderr, "Error saving image (%s): %s\n", fname, err );
		exit( 1 );
            }	
        } break;

	case OPTIX_PIXEL_FORMAT_FLOAT4:
        {
            const char* err;
            int32_t ret = SaveEXR(
                    reinterpret_cast<float*>( &data[0] ),
                    image.width,
                    image.height,
                    4, // num components
                    static_cast<int32_t>( fp16 ), // save_as_fp16
                    filename.c_str(),
                    &err );

            if( ret != TINYEXR_SUCCESS )
            {
                fprintf( stderr, "Error saving image (%s): %s\n", fname, err );
		exit( 1 );
            }
        } break;

        default:
        {
            fprintf(stderr, "Error saving image (%s): Unrecognized image buffer pixel format.\n", fname );
	    exit( 1 );
        }
    }
}

// filename is copied to result and the first sequence of "+" characters is
// replaced (using leading zeros) with framename.
// true is returned if the framenumber is -1 or if the function was successful.

static bool getFrameFilename( std::string& result, const std::string& filename, const std::string& colorFilename, int frame )
{
    if( filename.find( "exr" ) == std::string::npos )   // no ext extension, it must be a layer name, search in colorFilename
    {                                                   // which already has the name for the given frame
        result = colorFilename;
        return true;
    }

    result = filename;
    if( frame == -1 )
        return true;
    size_t nplus = 0;
    size_t ppos  = result.find( '+' );
    if( ppos == std::string::npos )
        return true;  // static filename without "+" characters
    size_t cpos = ppos;
    while( result[cpos] != 0 && result[cpos] == '+' )
    {
        nplus++;
        cpos++;
    }
    std::string fn = std::to_string( frame );
    if( fn.length() > nplus )
    {
        std::cerr << "illegal temporal filename, framenumber requires " << fn.length()
                  << " digits, \"+\" placeholder length: " << nplus << "too small" << std::endl;
        return false;
    }
    for( size_t i = 0; i < nplus; i++ )
        result[ppos + i] = '0';
    for( size_t i = 0; i < fn.length(); i++ )
        result[ppos + nplus - 1 - i] = fn[fn.length() - 1 - i];
    return true;
}

int32_t main( int32_t argc, char** argv )
{
    if( argc < 2 )
        printUsageAndExit( argv[0] );

    std::string              color_filename;
    std::string              beauty_filename;
    std::string              normal_filename;
    std::string              albedo_filename;
    std::string              flow_filename;
    std::string              flowtrust_filename;
    std::string              output_filename;
    std::vector<std::string> aov_filenames;
    bool                     applyFlowMode  = false;
    float                    exposure   = 0.f;
    int                      firstFrame = -1, lastFrame = -1;
    unsigned int             tileWidth = 0, tileHeight = 0;
    bool                     upscale2x = false;
    bool                     denoiseAlpha = false;
    bool                     specularMode = 0;
    float                    flowMulX = 1.f, flowMulY = 1.f;    // motion vectors are not scaled
    bool                     writeFP16 = true;

    for( int32_t i = 1; i < argc; ++i )
    {
        std::string arg( argv[i] );

        if( arg == "-b" || arg == "--beauty" )
        {
            if( i == argc - 1 )
                printUsageAndExit( argv[0] );
            beauty_filename = argv[++i];
        }
        else if( arg == "-n" || arg == "--normal" )
        {
            if( i == argc - 1 )
                printUsageAndExit( argv[0] );
            normal_filename = argv[++i];
        }
        else if( arg == "-a" || arg == "--albedo" )
        {
            if( i == argc - 1 )
                printUsageAndExit( argv[0] );
            albedo_filename = argv[++i];
        }
        else if( arg == "-e" || arg == "--exposure" )
        {
            if( i == argc - 1 )
                printUsageAndExit( argv[0] );
            exposure = std::stof( argv[++i] );
        }
        else if( arg == "-f" || arg == "--flow" )
        {
            if( i == argc - 1 )
                printUsageAndExit( argv[0] );
            flow_filename = argv[++i];
        }
        else if( arg == "-T" )
        {
            if( i == argc - 1 )
                printUsageAndExit( argv[0] );
            flowtrust_filename = argv[++i];
        }
        else if( arg == "-o" || arg == "--out" )
        {
            if( i == argc - 1 )
                printUsageAndExit( argv[0] );
            output_filename = argv[++i];
        }
        else if( arg == "-t" || arg == "--tilesize" )
        {
            try
            {
                size_t pos;
                if( i == argc - 1 )
                    printUsageAndExit( argv[0] );
                std::string s1( argv[++i] );
                tileWidth = unsigned( std::stoi( s1, &pos ) );
                if( pos != s1.length() )
                    printUsageAndExit( argv[0] );

                if( i == argc - 1 )
                    printUsageAndExit( argv[0] );
                std::string s2( argv[++i] );
                tileHeight = unsigned( std::stoi( s2, &pos ) );
                if( pos != s2.length() )
                    printUsageAndExit( argv[0] );
            } catch( ... )
            {
                printUsageAndExit( argv[0] );
            }
        }
        else if( arg == "-A" || arg == "--AOV" )
        {
            if( i == argc - 1 )
                printUsageAndExit( argv[0] );
            aov_filenames.push_back( std::string( argv[++i] ) );
        }
        else if( arg == "-S" )
        {
            if( i == argc - 1 )
                printUsageAndExit( argv[0] );
            aov_filenames.push_back( std::string( argv[++i] ) );
            specularMode = true;
        }
        else if( arg == "-fp32" )
        {
            writeFP16 = false;
        }
        else if( arg == "-z" )
        {
            applyFlowMode = true;
        }
        else if( arg == "-up2" )
        {
            upscale2x = true;
        }
        else if( arg == "-alpha" )
        {
            denoiseAlpha = true;
        }
        else if( arg == "-F" || arg == "--Frames" )
        {
            if( i == argc - 1 )
                printUsageAndExit( argv[0] );
            std::string s( argv[++i] );
            size_t cpos = s.find( '-' );
            if( cpos == std::string::npos )
            {
                size_t pos;
                try
                {
                    firstFrame = std::stoi( s, &pos );
                    if( pos != s.length() )
                        printUsageAndExit( argv[0] );

                    if( i == argc - 1 )
                        printUsageAndExit( argv[0] );
                    std::string s2( argv[++i] );
                    lastFrame = std::stoi( s2, &pos );
                    if( pos != s2.length() )
                        printUsageAndExit( argv[0] );
                } catch( ... )
                {
                    printUsageAndExit( argv[0] );
                }
            }
            else 
            {
                if( cpos == 0 || cpos == s.length() - 1 )
                    printUsageAndExit( argv[0] );
                firstFrame = atoi( s.substr( 0, cpos ).c_str() );
                lastFrame  = atoi( s.substr( cpos + 1 ).c_str() );
            }

            if( firstFrame < 0 || lastFrame < 0 || firstFrame > lastFrame )
            {
                fprintf( stderr, "Illegal frame range, first frame must be <= last frame and >= 0\n" );
                exit( 1 );
            }
        }
        else if( arg == "-fmul" )
        {
            try
            {
                size_t pos;
                if( i == argc - 1 )
                    printUsageAndExit( argv[0] );
                std::string s1( argv[++i] );
                flowMulX = std::stof( s1, &pos );
                if( pos != s1.length() )
                    printUsageAndExit( argv[0] );

                if( i == argc - 1 )
                    printUsageAndExit( argv[0] );
                std::string s2( argv[++i] );
                flowMulY = std::stof( s2, &pos );
                if( pos != s2.length() )
                    printUsageAndExit( argv[0] );
            } catch( ... )
            {
                printUsageAndExit( argv[0] );
            }
        }
        else if( arg[0] == '-' )
        {
            printUsageAndExit( argv[0] );
        }
        else
        {
            color_filename = arg;
        }
    }

    bool temporalMode = bool( firstFrame != -1 );

    if( temporalMode && flow_filename.empty() )
    {
        fprintf( stderr, "Temporal mode enabled, flow filename not specified\n" );
        exit( 1 );
    }

    OptixImage2D color     = {};
    OptixImage2D normal    = {};
    OptixImage2D albedo    = {};
    OptixImage2D flow      = {};
    OptixImage2D flowtrust = {};
    
    CUstream stream = 0;

    //
    // Initialize CUDA and create OptiX context
    //
    // Initialize CUDA
    if( cudaFree( nullptr ) != cudaSuccess )
    {
        fprintf(stderr, "CUDA initialization failed\n");
        exit( 1 );
    }

    CUcontext cu_ctx = nullptr;  // zero means take the current context
    if( optixInit() )
    {
        fprintf(stderr, "OptiX initialization failed\n");
        exit( 1 );
    }

    OptixDeviceContext context;
    OptixDeviceContextOptions co = {};
    co.logCallbackFunction       = &context_log_cb;
    co.logCallbackLevel          = 4;
    if( optixDeviceContextCreate( cu_ctx, &co, &context ) )
    {
        fprintf(stderr, "OptiX device context creation failed\n");
        exit( 1 );
    }

    OptiXDenoiser denoiser( context_log_cb, 0 );

    ApplyFlow applyFlow;

    for( int frame = firstFrame; frame <= lastFrame; frame++ )
    {
        EXRImage exrImage = {};                 // cached image if multilayer EXR given

        std::vector<OptixImage2D> aovs;

        printf( "Loading inputs " );
        if( frame != -1 )
            printf( "for frame %d", frame );
        printf( "\n" );

        std::string frame_filename;
        if( !getFrameFilename( frame_filename, color_filename, std::string(""), frame ) )
        {
            fprintf( stderr, "Error creating color filename for %s\n", color_filename.c_str() );
            exit( 1 );
        }
        std::string frame_filename_input = frame_filename;

        color = loadImageLayer( frame_filename.c_str(), beauty_filename.empty() ? 0 : beauty_filename.c_str(), &exrImage );
        printf( "Loaded color image %s, width %d, height %d\n", frame_filename.c_str(), color.width, color.height );

        if( !normal_filename.empty() )
        {
            if( !getFrameFilename( frame_filename, normal_filename, frame_filename_input, frame ) )
            {
                fprintf( stderr, "Error creating normal filename for %s\n", normal_filename.c_str() );
                exit( 0 );
            }

            // allocate four channels. only two/three channels used depending on model.
            normal = loadImageLayer( frame_filename.c_str(), normal_filename.c_str(), &exrImage );
            printf( "Loaded normal image %s\n", frame_filename.c_str() );
        }

        if( !albedo_filename.empty() )
        {
            if( !getFrameFilename( frame_filename, albedo_filename, frame_filename_input, frame ) )
            {
                fprintf( stderr, "Error creating albedo filename for %s\n", albedo_filename.c_str() );
                exit( 0 );
            }
            // allocate four channels. only three channels used.
            albedo = loadImageLayer( frame_filename.c_str(), albedo_filename.c_str(), &exrImage );
            printf( "Loaded albedo image %s\n", frame_filename.c_str() );
        }

        if( !flow_filename.empty() )
        {
            if( !getFrameFilename( frame_filename, flow_filename, frame_filename_input, frame ) )
            {
                fprintf( stderr, "Error creating flow filename for %s\n", flow_filename.c_str() );
                exit( 1 );
            }
            // allocate four channels. only two channels used.
            flow = loadImageLayer( frame_filename.c_str(), flow_filename.c_str(), &exrImage );
            printf( "Loaded flow image %s\n", frame_filename.c_str() );
        }

        if( !flowtrust_filename.empty() )
        {
            if( !getFrameFilename( frame_filename, flowtrust_filename, frame_filename_input, frame ) )
            {
                fprintf( stderr, "Error creating flowtrust filename for %s\n", flowtrust_filename.c_str() );
                exit( 1 );
            }
            // allocate four channels. only three channels used.
            flowtrust = loadImageLayer( frame_filename.c_str(), flowtrust_filename.c_str(), &exrImage );
            printf( "Loaded flowTrustworthiness image %s\n", frame_filename.c_str() );
        }

        for( size_t i = 0; i < aov_filenames.size(); i++ )
        {
            if( !getFrameFilename( frame_filename, aov_filenames[i], frame_filename_input, frame ) )
            {
                fprintf( stderr, "Error creating aov filename for %s\n", aov_filenames[i].c_str() );
                exit( 1 );
            }
            aovs.push_back( loadImageLayer( frame_filename.c_str(), aov_filenames[i].c_str(), &exrImage ) );
            printf( "Loaded aov image %s\n", frame_filename.c_str() );
        }

        if( frame == firstFrame )
        {
            bool ret;
            if( applyFlowMode )
                ret = applyFlow.init( color, stream );
            else
                ret = denoiser.init( context, stream, color.width, color.height, tileWidth, tileHeight,
                                     upscale2x,
                                     albedo.data != 0,
                                     normal.data != 0, 
                                     temporalMode,
                                     denoiseAlpha );
            if( !ret )
            {
                fprintf(stderr, "Error initializing denoiser\n");
                exit( 1 );
            }
        }

        OptiXDenoiser::InputData indata;
        indata.color     = color;
        indata.albedo    = albedo;
        indata.normal    = normal;
        indata.flow      = flow;
        indata.flowtrust = flowtrust;

        // set AOVs
        for( size_t i = 0; i < aovs.size(); i++ )
            indata.aovs.push_back( aovs[i] );

        unsigned int outScale = upscale2x ? 2 : 1;

        // allocate outputs
        OptiXDenoiser::OutputData outdata;
        outdata.color = createOptixImage2D( outScale * color.width, outScale * color.height, OPTIX_PIXEL_FORMAT_FLOAT4 );
        for( size_t i = 0; i < aovs.size(); i++ )
            outdata.aovs.push_back( createOptixImage2D( outScale * color.width, outScale * color.height, OPTIX_PIXEL_FORMAT_FLOAT4 ) );

        bool ret = true;

        if( frame == firstFrame && temporalMode )
        {
            if( cudaMemsetAsync( (void*)flow.data, 0, flow.rowStrideInBytes * flow.height, stream ) != cudaSuccess )
                ret = false;
        }

        printf( "Denoising ...\n" );

        const double t0 = getCurrentTime();
        if( applyFlowMode )
        {
            // apply current flow to previous frame noisy image with current flow
            ret = applyFlow.apply( outdata.color, color, flow, flowMulX, flowMulY, stream );
        }
        else
            ret = denoiser.denoise( outdata, indata, stream, flowMulX, flowMulY, frame == firstFrame );

        if( !ret )
        {
            fprintf( stderr, "Error during denoising\n" );
            exit( 1 );
        }

        cudaStreamSynchronize( stream );

        const double t1 = getCurrentTime();
        printf( "Denoise frame: %.2f ms\n", ( t1 - t0 ) * 1000.f );

        if( !output_filename.empty() )
        {
            // AOVs are not written when speclarMode is set. A single specular AOV is expected in this mode,
            // to keep the sample code simple.
            size_t numOutputs = specularMode ? 1 : 1 + aovs.size();

            for( size_t i = 0; i < numOutputs; i++ )
            {
                frame_filename = output_filename;
                getFrameFilename( frame_filename, output_filename, std::string(""), frame );
                if( i > 0 )
                {
                    std::string basename = aov_filenames[i - 1].substr( aov_filenames[i - 1].find_last_of( "/\\" ) + 1 );
                    std::string::size_type const p( basename.find_last_of( '.' ) );
                    std::string                  b = basename.substr( 0, p );
                    frame_filename.insert( frame_filename.rfind( '.' ), "_" + b + "_denoised" );
                }
                OptixImage2D& image = ( i == 0 ) ? outdata.color : outdata.aovs[i-1];
                if( exposure != 0.f )
                {
                    std::vector<char> data( image.rowStrideInBytes * image.height );
                    cudaMemcpy( (void*)&data[0], (void*)image.data, image.rowStrideInBytes * image.height, cudaMemcpyDeviceToHost );
                    for( unsigned int p = 0; p < image.width * image.height; p++ )
                    {
                        float* f = &((float*)&data[0])[p * 4 + 0];
                        f[0] *= std::pow( 2.f, exposure );
                        f[1] *= std::pow( 2.f, exposure );
                        f[2] *= std::pow( 2.f, exposure );
                    }
                    cudaMemcpy( (void*)image.data, &data[0], image.rowStrideInBytes * image.height, cudaMemcpyHostToDevice );
                }
                printf( "Saving results to %s\n", frame_filename.c_str() );
                saveImageEXR( frame_filename.c_str(), image, writeFP16 );
            }
        }

        freeOptixImage2D( color );
        freeOptixImage2D( albedo );
        freeOptixImage2D( normal );
        freeOptixImage2D( flow );
        freeOptixImage2D( flowtrust );

        for( size_t i = 0; i < aovs.size(); i++ )
            freeOptixImage2D( aovs[i] );

        freeOptixImage2D( outdata.color );
        for( size_t i = 0; i < aovs.size(); i++ )
            freeOptixImage2D( outdata.aovs[i] );

        if( exrImage.num_channels > 0 )
        {
            FreeEXRImage( &exrImage );
            exrImage = {};
        }
    }

    applyFlow.exit();

    denoiser.exit();
    optixDeviceContextDestroy( context );

    cudaError_t error = cudaGetLastError();
    if( error != cudaSuccess )
    {
        fprintf( stderr, "Error during denoising: %s\n", cudaGetErrorString( error ) );
        exit( 1 );
    }

    return 0;
}
