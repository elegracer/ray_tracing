cmake_minimum_required(VERSION 3.14)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules")
project(ray_tracing LANGUAGES C CXX CUDA VERSION 0.0.1.0)

if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 86 89)
endif()

include(CMakeDependentOption)

find_program(CCACHE ccache)
if(CCACHE)
    message(STATUS "ccache found")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
else()
    message(STATUS "ccache not found")
endif()

# Set the output directory for libraries and executables
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)

# set global cxx standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
# always generate position independent code, no matter static or shared library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# disable cpp extensions
set(CMAKE_CXX_EXTENSIONS OFF)
# # link time optimization
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT ANDROID)
#     add_compile_options(-stdlib=libc++)
#     add_link_options(-stdlib=libc++)
#     set(CMAKE_CXX_FLAGS "-stdlib=libc++")
# endif()

set(THIRDPARTY_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/3rdparty)

find_package(Eigen3 CONFIG REQUIRED)
find_package(CUDAToolkit REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(range-v3 CONFIG REQUIRED)
find_package(msft_proxy4 CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_path(ICECREAM_CPP_INCLUDE_DIRS "icecream.hpp")
find_package(TBB CONFIG REQUIRED)
find_package(indicators CONFIG REQUIRED)
find_path(OPTIX_INCLUDE_DIR optix.h
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64
    PATH_SUFFIXES include
    REQUIRED
)

# options
option(OPTION_A "..." ON)

enable_testing()

# core library
configure_file(cmake/version.h.in include/core/version.h)

add_library(core SHARED)
target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/include/core/version.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/core.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/core.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/camera_models.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/camera_rig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/profiling/benchmark_report.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/scene_description.cpp
)
target_include_directories(core
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ICECREAM_CPP_INCLUDE_DIRS}
)
target_link_libraries(core
    PUBLIC
        Eigen3::Eigen
        fmt::fmt
        argparse::argparse
        range-v3::meta range-v3::concepts range-v3::range-v3
        msft_proxy4::proxy
        ${OpenCV_LIBS}
        TBB::tbb TBB::tbbmalloc
        indicators::indicators
)

# utils
add_executable(estimate_halfway)
target_sources(estimate_halfway PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/estimate_halfway.cpp)
target_link_libraries(estimate_halfway PRIVATE core)

add_executable(estimate_pi)
target_sources(estimate_pi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/estimate_pi.cpp)
target_link_libraries(estimate_pi PRIVATE core)

add_executable(integrate_x_sq)
target_sources(integrate_x_sq PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/integrate_x_sq.cpp)
target_link_libraries(integrate_x_sq PRIVATE core)

add_executable(render_scene)
target_sources(render_scene PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/render_scene.cpp)
target_link_libraries(render_scene PRIVATE core)

add_executable(sphere_importance)
target_sources(sphere_importance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/sphere_importance.cpp)
target_link_libraries(sphere_importance PRIVATE core)

add_executable(cos_cubed)
target_sources(cos_cubed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/cos_cubed.cpp)
target_link_libraries(cos_cubed PRIVATE core)

add_executable(cos_density)
target_sources(cos_density PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/cos_density.cpp)
target_link_libraries(cos_density PRIVATE core)

add_executable(test_cpu_render_smoke)
target_sources(test_cpu_render_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cpu_render_smoke.cpp)
target_link_libraries(test_cpu_render_smoke PRIVATE core)
add_test(NAME test_cpu_render_smoke COMMAND test_cpu_render_smoke)

add_executable(test_frame_convention)
target_sources(test_frame_convention PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_frame_convention.cpp)
target_link_libraries(test_frame_convention PRIVATE core)
add_test(NAME test_frame_convention COMMAND test_frame_convention)

add_executable(test_camera_models)
target_sources(test_camera_models
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_camera_models.cpp
)
target_link_libraries(test_camera_models PRIVATE core)
add_test(NAME test_camera_models COMMAND test_camera_models)

add_executable(test_camera_rig)
target_sources(test_camera_rig
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_camera_rig.cpp
)
target_link_libraries(test_camera_rig PRIVATE core)
add_test(NAME test_camera_rig COMMAND test_camera_rig)

add_executable(test_scene_description)
target_sources(test_scene_description
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene_description.cpp
)
target_link_libraries(test_scene_description PRIVATE core)
add_test(NAME test_scene_description COMMAND test_scene_description)

add_executable(test_render_profile)
target_sources(test_render_profile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_render_profile.cpp)
target_link_libraries(test_render_profile PRIVATE core)
add_test(NAME test_render_profile COMMAND test_render_profile)

add_executable(test_realtime_benchmark_report)
target_sources(test_realtime_benchmark_report
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_realtime_benchmark_report.cpp
)
target_link_libraries(test_realtime_benchmark_report PRIVATE core)
add_test(NAME test_realtime_benchmark_report COMMAND test_realtime_benchmark_report)

add_library(realtime_gpu STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/optix_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/programs.cu
)
target_include_directories(realtime_gpu PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPTIX_INCLUDE_DIR}
)
target_link_libraries(realtime_gpu PUBLIC
    core
    CUDA::cudart
    CUDA::cuda_driver
    dl
)

add_executable(test_optix_direction)
target_sources(test_optix_direction
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_direction.cpp
)
target_link_libraries(test_optix_direction PRIVATE realtime_gpu)
add_test(NAME test_optix_direction COMMAND test_optix_direction)

add_executable(test_optix_path_trace)
target_sources(test_optix_path_trace
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_path_trace.cpp
)
target_link_libraries(test_optix_path_trace PRIVATE realtime_gpu)
add_test(NAME test_optix_path_trace COMMAND test_optix_path_trace)

add_executable(test_optix_profiled_render)
target_sources(test_optix_profiled_render
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_profiled_render.cpp
)
target_link_libraries(test_optix_profiled_render PRIVATE realtime_gpu)
add_test(NAME test_optix_profiled_render COMMAND test_optix_profiled_render)

add_executable(test_optix_prepared_scene)
target_sources(test_optix_prepared_scene
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_prepared_scene.cpp
)
target_link_libraries(test_optix_prepared_scene PRIVATE realtime_gpu)
add_test(NAME test_optix_prepared_scene COMMAND test_optix_prepared_scene)

add_executable(test_renderer_pool)
target_sources(test_renderer_pool
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_renderer_pool.cpp
)
target_link_libraries(test_renderer_pool PRIVATE realtime_gpu)
add_test(NAME test_renderer_pool COMMAND test_renderer_pool)

add_executable(test_optix_materials_aux)
target_sources(test_optix_materials_aux
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_materials_aux.cpp
)
target_link_libraries(test_optix_materials_aux PRIVATE realtime_gpu)
add_test(NAME test_optix_materials_aux COMMAND test_optix_materials_aux)

add_executable(test_optix_equi_path_trace)
target_sources(test_optix_equi_path_trace
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_equi_path_trace.cpp
)
target_link_libraries(test_optix_equi_path_trace PRIVATE realtime_gpu)
add_test(NAME test_optix_equi_path_trace COMMAND test_optix_equi_path_trace)

target_sources(realtime_gpu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/denoiser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/renderer_pool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/realtime_pipeline.cpp
)

add_executable(render_realtime)
target_sources(render_realtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/render_realtime.cpp)
target_link_libraries(render_realtime PRIVATE realtime_gpu core)
add_test(NAME test_render_realtime_cli
    COMMAND $<TARGET_FILE:render_realtime>
        --camera-count 1
        --frames 1
        --profile realtime
        --output-dir ${CMAKE_CURRENT_BINARY_DIR}/test/render_realtime-cli
)
add_test(NAME test_render_realtime_profiling_cli
    COMMAND ${CMAKE_COMMAND}
        -DRENDER_REALTIME_EXE=$<TARGET_FILE:render_realtime>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test/render_realtime-profiling
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRenderRealtimeProfiling.cmake
)
add_test(NAME test_render_realtime_four_camera_profiling_cli
    COMMAND ${CMAKE_COMMAND}
        -DRENDER_REALTIME_EXE=$<TARGET_FILE:render_realtime>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test/render_realtime-four-camera
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
)
add_test(NAME test_render_realtime_four_camera_quality_cli
    COMMAND ${CMAKE_COMMAND}
        -DRENDER_REALTIME_EXE=$<TARGET_FILE:render_realtime>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test/render_realtime-four-camera-quality
        -DPROFILE_NAME=quality
        -DEXPECT_DENOISE_ENABLED=OFF
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
)
add_test(NAME test_render_realtime_final_room_quality_cli
    COMMAND ${CMAKE_COMMAND}
        -DRENDER_REALTIME_EXE=$<TARGET_FILE:render_realtime>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test/render_realtime-final-room-quality
        -DSCENE_NAME=final_room
        -DFRAME_COUNT=1
        -DPROFILE_NAME=quality
        -DEXPECT_DENOISE_ENABLED=OFF
        -DEXPECT_MIN_CAMERA_LUMINANCE_SPREAD_MILLI=10
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
)
add_test(NAME test_render_realtime_cli_skip_write
    COMMAND ${CMAKE_COMMAND}
        -DRENDER_REALTIME_EXE=$<TARGET_FILE:render_realtime>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test/render_realtime-skip-write
        -DEXPECT_SKIP_WRITE=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRenderRealtimeProfiling.cmake
)

add_executable(test_realtime_pipeline)
target_sources(test_realtime_pipeline
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_realtime_pipeline.cpp
)
target_link_libraries(test_realtime_pipeline PRIVATE realtime_gpu)
add_test(NAME test_realtime_pipeline COMMAND test_realtime_pipeline)

add_executable(test_reference_vs_realtime)
target_sources(test_reference_vs_realtime
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_reference_vs_realtime.cpp
)
target_link_libraries(test_reference_vs_realtime PRIVATE realtime_gpu core)
add_test(NAME test_reference_vs_realtime COMMAND test_reference_vs_realtime)
