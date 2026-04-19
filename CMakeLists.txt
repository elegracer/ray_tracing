cmake_minimum_required(VERSION 3.14)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules")
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg_root/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
endif()
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
find_package(yaml-cpp CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_path(OPTIX_INCLUDE_DIR optix.h
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64
    PATH_SUFFIXES include
    REQUIRED
)

# options
option(OPTION_A "..." ON)
option(ENABLE_GUI_VIEWER "Build the interactive GLFW/OpenGL viewer" ON)

if(ENABLE_GUI_VIEWER)
    find_package(imgui CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
    set(GLFW_TARGET glfw)
    if(TARGET glfw::glfw)
        set(GLFW_TARGET glfw::glfw)
    elseif(NOT TARGET glfw)
        message(FATAL_ERROR "GLFW package did not export a glfw target")
    endif()
endif()

enable_testing()

# core library
configure_file(cmake/version.h.in include/core/version.h)

add_library(core SHARED)
target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/include/core/version.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/core.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/core.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/offline_shared_scene_renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/offline_shared_scene_renderer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/cpu_scene_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/obj_mtl_importer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/cpu_scene_adapter.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/realtime_scene_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/realtime_scene_adapter.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_definition.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/yaml_scene_loader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/shared_scene_ir.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/shared_scene_builders.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/camera_models.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/camera_rig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/profiling/benchmark_report.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/scene_catalog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/realtime_scene_factory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/scene_description.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/body_pose.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/default_viewer_scene.cpp
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
    PRIVATE
        yaml-cpp::yaml-cpp
        tinyobjloader::tinyobjloader
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

if(ENABLE_GUI_VIEWER)
    add_executable(render_realtime_viewer)
    target_sources(render_realtime_viewer
        PRIVATE
            utils/render_realtime_viewer.cpp
            src/realtime/viewer/four_camera_rig.cpp
            src/realtime/viewer/move_speed.cpp
            src/realtime/viewer/scene_switch_controller.cpp
            src/realtime/viewer/viewer_quality_controller.cpp
    )
    target_link_libraries(render_realtime_viewer PRIVATE realtime_gpu core imgui::imgui ${GLFW_TARGET} OpenGL::GL)
endif()

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

add_executable(test_viewer_body_pose)
target_sources(test_viewer_body_pose
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_viewer_body_pose.cpp
)
target_link_libraries(test_viewer_body_pose PRIVATE core)
add_test(NAME test_viewer_body_pose COMMAND test_viewer_body_pose)

add_executable(test_viewer_four_camera_rig)
target_sources(test_viewer_four_camera_rig
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_viewer_four_camera_rig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/four_camera_rig.cpp
)
target_link_libraries(test_viewer_four_camera_rig PRIVATE core)
add_test(NAME test_viewer_four_camera_rig COMMAND test_viewer_four_camera_rig)

add_executable(test_viewer_scene_switch_controller)
target_sources(test_viewer_scene_switch_controller
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_viewer_scene_switch_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/scene_switch_controller.cpp
)
target_link_libraries(test_viewer_scene_switch_controller PRIVATE core)
add_test(NAME test_viewer_scene_switch_controller COMMAND test_viewer_scene_switch_controller)

add_executable(test_viewer_move_speed)
target_sources(test_viewer_move_speed
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_viewer_move_speed.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/move_speed.cpp
)
target_link_libraries(test_viewer_move_speed PRIVATE core)
add_test(NAME test_viewer_move_speed COMMAND test_viewer_move_speed)

add_executable(test_viewer_quality_controller)
target_sources(test_viewer_quality_controller
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_viewer_quality_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/viewer_quality_controller.cpp
)
target_link_libraries(test_viewer_quality_controller PRIVATE core)
add_test(NAME test_viewer_quality_controller COMMAND test_viewer_quality_controller)

add_executable(test_display_transfer)
target_sources(test_display_transfer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_display_transfer.cpp
)
target_link_libraries(test_display_transfer PRIVATE core)
add_test(NAME test_display_transfer COMMAND test_display_transfer)

add_executable(test_viewer_quality_reference)
target_sources(test_viewer_quality_reference
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_viewer_quality_reference.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/four_camera_rig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/viewer_quality_controller.cpp
)
target_link_libraries(test_viewer_quality_reference PRIVATE realtime_gpu core)
add_test(NAME test_viewer_quality_reference COMMAND test_viewer_quality_reference)

add_executable(test_scene_description)
target_sources(test_scene_description
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene_description.cpp
)
target_link_libraries(test_scene_description PRIVATE core)
add_test(NAME test_scene_description COMMAND test_scene_description)

add_executable(test_scene_definition)
target_sources(test_scene_definition
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene_definition.cpp
)
target_link_libraries(test_scene_definition PRIVATE core)
add_test(NAME test_scene_definition COMMAND test_scene_definition)

add_executable(test_yaml_scene_loader)
target_sources(test_yaml_scene_loader
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_yaml_scene_loader.cpp
)
target_link_libraries(test_yaml_scene_loader PRIVATE core)
add_test(NAME test_yaml_scene_loader COMMAND test_yaml_scene_loader)

add_executable(test_obj_mtl_importer)
target_sources(test_obj_mtl_importer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_obj_mtl_importer.cpp
)
target_link_libraries(test_obj_mtl_importer PRIVATE core)
add_test(NAME test_obj_mtl_importer COMMAND test_obj_mtl_importer)

add_executable(test_realtime_texture_materials)
target_sources(test_realtime_texture_materials
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_realtime_texture_materials.cpp
)
target_link_libraries(test_realtime_texture_materials PRIVATE realtime_gpu)
add_test(NAME test_realtime_texture_materials COMMAND test_realtime_texture_materials)

add_executable(test_shared_scene_ir)
target_sources(test_shared_scene_ir
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shared_scene_ir.cpp
)
target_link_libraries(test_shared_scene_ir PRIVATE core)
add_test(NAME test_shared_scene_ir COMMAND test_shared_scene_ir)

add_executable(test_shared_scene_builders)
target_sources(test_shared_scene_builders
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shared_scene_builders.cpp
)
target_link_libraries(test_shared_scene_builders PRIVATE core)
add_test(NAME test_shared_scene_builders COMMAND test_shared_scene_builders)

add_executable(test_shared_scene_regression)
target_sources(test_shared_scene_regression
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shared_scene_regression.cpp
)
target_link_libraries(test_shared_scene_regression PRIVATE core)
add_test(NAME test_shared_scene_regression COMMAND test_shared_scene_regression)

add_executable(test_cpu_scene_adapter)
target_sources(test_cpu_scene_adapter
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cpu_scene_adapter.cpp
)
target_link_libraries(test_cpu_scene_adapter PRIVATE core)
add_test(NAME test_cpu_scene_adapter COMMAND test_cpu_scene_adapter)

add_executable(test_offline_shared_scene_renderer)
target_sources(test_offline_shared_scene_renderer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_offline_shared_scene_renderer.cpp
)
target_link_libraries(test_offline_shared_scene_renderer PRIVATE core)
add_test(NAME test_offline_shared_scene_renderer COMMAND test_offline_shared_scene_renderer)

add_executable(test_scene_catalog)
target_sources(test_scene_catalog
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene_catalog.cpp
)
target_link_libraries(test_scene_catalog PRIVATE core)
add_test(NAME test_scene_catalog COMMAND test_scene_catalog)

add_executable(test_realtime_scene_factory)
target_sources(test_realtime_scene_factory
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_realtime_scene_factory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/four_camera_rig.cpp
)
target_link_libraries(test_realtime_scene_factory PRIVATE core)
add_test(NAME test_realtime_scene_factory COMMAND test_realtime_scene_factory)

add_executable(test_realtime_scene_adapter)
target_sources(test_realtime_scene_adapter
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_realtime_scene_adapter.cpp
)
target_link_libraries(test_realtime_scene_adapter PRIVATE core)
add_test(NAME test_realtime_scene_adapter COMMAND test_realtime_scene_adapter)

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
