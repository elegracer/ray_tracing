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
find_package(Sophus CONFIG REQUIRED)
find_path(OPTIX_INCLUDE_DIR optix.h
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64
    PATH_SUFFIXES include
    REQUIRED
)

if(CMAKE_CONFIGURATION_TYPES)
    set(RT_BUILD_CONFIGURATION "$<CONFIG>")
elseif(CMAKE_BUILD_TYPE)
    set(RT_BUILD_CONFIGURATION "${CMAKE_BUILD_TYPE}")
else()
    set(RT_BUILD_CONFIGURATION "single-config-default")
endif()

# options
option(OPTION_A "..." ON)
option(ENABLE_GUI_VIEWER "Build the interactive GLFW/OpenGL viewer" ON)
option(RT_ENABLE_OPENUSD "Build the official OpenUSD stage importer" OFF)
option(RT_ENABLE_PUBLIC_ACCEPTANCE_PROBES
    "Run version-pinned public USD and OpenPBR product acceptance probes" OFF)

if(RT_ENABLE_PUBLIC_ACCEPTANCE_PROBES AND NOT RT_ENABLE_OPENUSD)
    message(FATAL_ERROR "RT_ENABLE_PUBLIC_ACCEPTANCE_PROBES requires RT_ENABLE_OPENUSD=ON")
endif()

if(RT_ENABLE_PUBLIC_ACCEPTANCE_PROBES)
    find_package(MaterialX CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(OpenEXR REQUIRED)
endif()

if(RT_ENABLE_OPENUSD)
    find_package(pxr CONFIG REQUIRED)
    if(TARGET usd_m)
        set(RT_OPENUSD_TARGETS usd_m)
    elseif(TARGET usd AND TARGET usdGeom AND TARGET usdShade AND TARGET usdLux)
        set(RT_OPENUSD_TARGETS usd usdGeom usdShade usdLux)
    else()
        message(FATAL_ERROR
            "OpenUSD pxr package did not export usd_m or usd/usdGeom/usdShade/usdLux targets")
    endif()
    message(STATUS "OpenUSD stage import enabled with pxr ${PXR_VERSION}")
endif()

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

set(RT_PUBLIC_ACCEPTANCE_ASSET_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/.cache/public-acceptance-assets"
    CACHE PATH "Local cache for version-pinned public USD and OpenPBR acceptance assets")
set(RT_PUBLIC_ACCEPTANCE_ASSET_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/public_assets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/public_assets.lock.cmake")
add_custom_target(fetch_public_acceptance_assets
    COMMAND ${CMAKE_COMMAND}
        -DMODE=fetch
        -DROOT=${RT_PUBLIC_ACCEPTANCE_ASSET_ROOT}
        -P ${RT_PUBLIC_ACCEPTANCE_ASSET_SCRIPT}
    COMMENT "Fetching version-pinned public USD and OpenPBR acceptance assets"
    VERBATIM
)
add_custom_target(verify_public_acceptance_assets
    COMMAND ${CMAKE_COMMAND}
        -DMODE=verify
        -DROOT=${RT_PUBLIC_ACCEPTANCE_ASSET_ROOT}
        -P ${RT_PUBLIC_ACCEPTANCE_ASSET_SCRIPT}
    COMMENT "Verifying public USD and OpenPBR acceptance assets"
    VERBATIM
)
add_test(NAME test_public_acceptance_asset_lock
    COMMAND ${CMAKE_COMMAND} -DMODE=lock -P ${RT_PUBLIC_ACCEPTANCE_ASSET_SCRIPT})

# OpenUSD publishes a C++17 ABI and its generated schema headers are not
# compatible with libstdc++ constrained-pair checks under C++20/23. Keep the
# SDK-facing translation unit at the upstream language boundary while the
# renderer remains C++23.
add_library(openusd_stage_io OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/openusd_stage_importer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/openusd_stage_importer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/openusd_stage_exporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/openusd_stage_exporter.h
)
set_target_properties(openusd_stage_io PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_include_directories(openusd_stage_io PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(openusd_stage_io PRIVATE
    RT_HAS_OPENUSD=$<BOOL:${RT_ENABLE_OPENUSD}>
)
target_link_libraries(openusd_stage_io PRIVATE Eigen3::Eigen)
if(RT_ENABLE_OPENUSD)
    target_link_libraries(openusd_stage_io PRIVATE ${RT_OPENUSD_TARGETS})
endif()

# core library
configure_file(cmake/version.h.in include/core/version.h)

add_library(core SHARED)
target_sources(core
    PRIVATE
        $<TARGET_OBJECTS:openusd_stage_io>
        ${CMAKE_CURRENT_BINARY_DIR}/include/core/version.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/core.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/core.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/offline_shared_scene_renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/offline_shared_scene_renderer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/openpbr_core.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/analytic_light.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/cpu_analytic_light.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/cpu_analytic_light.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/analytic_light_compiler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/analytic_light_compiler.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/cpu_scene_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/obj_mtl_importer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/cpu_scene_adapter.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/openpbr_core_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/openpbr_core_adapter.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/realtime_scene_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/realtime_scene_adapter.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_file_catalog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_definition.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_ir_v2.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_ir_v2.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_ir_validator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_ir_validator.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/yaml_scene_loader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/shared_scene_ir.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/shared_scene_builders.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/camera_models.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/camera_projection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/camera_projection.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/camera_rig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/profiling/benchmark_report.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/render_profile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/render_profile.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/scene_catalog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/realtime_scene_factory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/scene_description.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/body_pose.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/default_viewer_scene.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/four_camera_rig.cpp
)
target_include_directories(core
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ICECREAM_CPP_INCLUDE_DIRS}
)
target_compile_definitions(core PUBLIC RT_HAS_OPENUSD=$<BOOL:${RT_ENABLE_OPENUSD}>)
target_link_libraries(core
    PUBLIC
        Eigen3::Eigen
        Sophus::Sophus
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
if(RT_ENABLE_OPENUSD)
    target_link_libraries(core PRIVATE ${RT_OPENUSD_TARGETS})
endif()
if(RT_ENABLE_PUBLIC_ACCEPTANCE_PROBES)
    target_sources(core PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/materialx_openpbr_loader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/materialx_openpbr_loader.h
    )
    target_link_libraries(core PRIVATE MaterialXCore MaterialXFormat)
endif()

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

add_executable(derive_default_camera_intrinsics)
target_sources(derive_default_camera_intrinsics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils/derive_default_camera_intrinsics.cpp)
target_link_libraries(derive_default_camera_intrinsics PRIVATE core)

if(ENABLE_GUI_VIEWER)
    add_library(viewer_cuda_gl STATIC
        src/realtime/viewer/cuda_gl_presenter.cu
        src/realtime/viewer/cuda_gl_presenter.h
    )
    target_include_directories(viewer_cuda_gl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(viewer_cuda_gl PRIVATE GL_GLEXT_PROTOTYPES)
    target_link_libraries(viewer_cuda_gl PUBLIC realtime_gpu CUDA::cudart OpenGL::GL)

    add_executable(render_realtime_viewer)
    target_sources(render_realtime_viewer
        PRIVATE
            utils/render_realtime_viewer.cpp
            src/realtime/viewer/four_camera_rig.cpp
            src/realtime/viewer/move_speed.cpp
            src/realtime/viewer/scene_switch_controller.cpp
            src/realtime/viewer/viewer_quality_controller.cpp
            src/realtime/viewer/viewer_render_session.cpp
    )
    target_link_libraries(render_realtime_viewer PRIVATE viewer_cuda_gl realtime_gpu core imgui::imgui ${GLFW_TARGET} OpenGL::GL)

    add_executable(test_cuda_gl_presenter tests/test_cuda_gl_presenter.cpp)
    target_link_libraries(test_cuda_gl_presenter PRIVATE viewer_cuda_gl ${GLFW_TARGET} OpenGL::GL)
    add_test(NAME test_cuda_gl_presenter COMMAND test_cuda_gl_presenter)
    set_tests_properties(test_cuda_gl_presenter PROPERTIES SKIP_RETURN_CODE 77)
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
target_include_directories(test_camera_models
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/reference/src-cam
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/reference_camera_shims
)
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

add_executable(test_viewer_scene_reload)
target_sources(test_viewer_scene_reload
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_viewer_scene_reload.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/scene_switch_controller.cpp
)
target_link_libraries(test_viewer_scene_reload PRIVATE core)
add_test(NAME test_viewer_scene_reload COMMAND test_viewer_scene_reload)

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

add_executable(test_viewer_render_session)
target_sources(test_viewer_render_session
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_viewer_render_session.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/viewer_quality_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/viewer_render_session.cpp
)
target_link_libraries(test_viewer_render_session PRIVATE core)
add_test(NAME test_viewer_render_session COMMAND test_viewer_render_session)

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

add_executable(test_scene_file_catalog)
target_sources(test_scene_file_catalog
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene_file_catalog.cpp
)
target_link_libraries(test_scene_file_catalog PRIVATE core)
add_test(NAME test_scene_file_catalog COMMAND test_scene_file_catalog)

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

add_executable(test_scene_ir_validator)
target_sources(test_scene_ir_validator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene_ir_validator.cpp
)
target_link_libraries(test_scene_ir_validator PRIVATE core)
add_test(NAME test_scene_ir_validator COMMAND test_scene_ir_validator)

add_executable(test_scene_ir_v2)
target_sources(test_scene_ir_v2
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene_ir_v2.cpp
)
target_link_libraries(test_scene_ir_v2 PRIVATE core)
add_test(NAME test_scene_ir_v2 COMMAND test_scene_ir_v2)

add_executable(test_openusd_stage_importer)
target_sources(test_openusd_stage_importer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_openusd_stage_importer.cpp
)
target_link_libraries(test_openusd_stage_importer PRIVATE core)
add_test(NAME test_openusd_stage_importer COMMAND test_openusd_stage_importer
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/openusd/composed_stage.usda)

add_executable(test_public_usd_scene_import)
target_sources(test_public_usd_scene_import
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_public_usd_scene_import.cpp
)
target_link_libraries(test_public_usd_scene_import PRIVATE core)
if(RT_ENABLE_PUBLIC_ACCEPTANCE_PROBES)
    string(CONCAT public_usd_acceptance_stage
        "${RT_PUBLIC_ACCEPTANCE_ASSET_ROOT}/"
        "${RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_CACHE_DIR}/"
        "${RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_ENTRYPOINT}")
    add_test(NAME test_public_acceptance_asset_integrity
        COMMAND ${CMAKE_COMMAND}
            -DMODE=verify
            -DROOT=${RT_PUBLIC_ACCEPTANCE_ASSET_ROOT}
            -P ${RT_PUBLIC_ACCEPTANCE_ASSET_SCRIPT})
    add_test(NAME test_public_usd_scene_import COMMAND test_public_usd_scene_import
        ${public_usd_acceptance_stage})
    string(CONCAT public_openpbr_acceptance_examples
        "${RT_PUBLIC_ACCEPTANCE_ASSET_ROOT}/"
        "${RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_CACHE_DIR}/examples")
    add_executable(test_public_openpbr_materials
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_public_openpbr_materials.cpp)
    target_link_libraries(test_public_openpbr_materials PRIVATE core)
    add_test(NAME test_public_openpbr_materials COMMAND test_public_openpbr_materials
        ${public_openpbr_acceptance_examples})
    set_tests_properties(
        test_public_acceptance_asset_integrity
        test_public_usd_scene_import
        test_public_openpbr_materials
        PROPERTIES LABELS "acceptance;public-assets")
endif()

add_executable(test_openusd_stage_exporter)
target_sources(test_openusd_stage_exporter
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_openusd_stage_exporter.cpp
)
target_link_libraries(test_openusd_stage_exporter PRIVATE core)
add_test(NAME test_openusd_stage_exporter COMMAND test_openusd_stage_exporter
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/openusd/composed_stage.usda)

add_executable(test_openpbr_core)
target_sources(test_openpbr_core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_openpbr_core.cpp
)
target_link_libraries(test_openpbr_core PRIVATE core)
add_test(NAME test_openpbr_core COMMAND test_openpbr_core)

add_executable(test_light_sampling)
target_sources(test_light_sampling
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_light_sampling.cpp
)
target_link_libraries(test_light_sampling PRIVATE core)
add_test(NAME test_light_sampling COMMAND test_light_sampling)

add_executable(test_analytic_light_quantitative)
target_sources(test_analytic_light_quantitative
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_analytic_light_quantitative.cpp
)
target_link_libraries(test_analytic_light_quantitative PRIVATE realtime_gpu)
add_test(NAME test_analytic_light_quantitative COMMAND test_analytic_light_quantitative)
set_tests_properties(test_analytic_light_quantitative PROPERTIES LABELS "physical;gpu")

add_executable(test_analytic_light_compiler)
target_sources(test_analytic_light_compiler
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_analytic_light_compiler.cpp
)
target_link_libraries(test_analytic_light_compiler PRIVATE realtime_gpu)
add_test(NAME test_analytic_light_compiler COMMAND test_analytic_light_compiler)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/cuda_event_timer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/cuda_event_timer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/device_frame_buffers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/device_frame_buffers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/device_radiance_frame_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/device_scene_buffers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/device_scene_buffers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/direction_debug_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/direction_debug_renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/host_radiance_staging.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/host_radiance_staging.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/optix_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/packed_scene_preparation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/packed_scene_preparation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/radiance_frame_assembly.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/radiance_frame_assembly.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/radiance_launch_setup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/radiance_launch_setup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/render_request_validation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/render_request_validation.h
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

if(RT_ENABLE_PUBLIC_ACCEPTANCE_PROBES)
    add_executable(render_public_acceptance
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/render_public_acceptance.cpp)
    target_compile_definitions(render_public_acceptance PRIVATE
        RT_ACCEPTANCE_USD_REVISION="${RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_REVISION}"
        RT_ACCEPTANCE_OPENPBR_REVISION="${RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_REVISION}"
        RT_ACCEPTANCE_USD_ENTRYPOINT="${RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_ENTRYPOINT}"
    )
    target_link_libraries(render_public_acceptance PRIVATE
        realtime_gpu core OpenSSL::Crypto OpenEXR::OpenEXR)
    set(public_acceptance_reference_dir
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/references/public_acceptance")
    set(public_acceptance_render_output_dir
        "${CMAKE_CURRENT_BINARY_DIR}/public-acceptance-render")
    add_test(NAME test_public_render_acceptance COMMAND render_public_acceptance
        --stage ${public_usd_acceptance_stage}
        --openpbr-dir ${public_openpbr_acceptance_examples}
        --output-dir ${public_acceptance_render_output_dir}
        --reference-dir ${public_acceptance_reference_dir})
    set_tests_properties(test_public_render_acceptance PROPERTIES
        LABELS "acceptance;public-assets;gpu"
        TIMEOUT 900)
    add_custom_target(approve_public_acceptance_references
        COMMAND render_public_acceptance
            --stage ${public_usd_acceptance_stage}
            --openpbr-dir ${public_openpbr_acceptance_examples}
            --output-dir ${public_acceptance_render_output_dir}
            --reference-dir ${public_acceptance_reference_dir}
            --approve-references
        DEPENDS render_public_acceptance
        COMMENT "Rendering and approving the pinned public acceptance references"
        VERBATIM)
endif()

add_executable(test_optix_direction)
target_sources(test_optix_direction
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_direction.cpp
)
target_link_libraries(test_optix_direction PRIVATE realtime_gpu)
add_test(NAME test_optix_direction COMMAND test_optix_direction)

add_executable(test_gpu_scene_preparation)
target_sources(test_gpu_scene_preparation
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gpu_scene_preparation.cpp
)
target_link_libraries(test_gpu_scene_preparation PRIVATE realtime_gpu)
add_test(NAME test_gpu_scene_preparation COMMAND test_gpu_scene_preparation)

add_executable(test_openpbr_core_cuda)
target_sources(test_openpbr_core_cuda
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_openpbr_core_cuda.cu
)
target_include_directories(test_openpbr_core_cuda PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_openpbr_core_cuda PRIVATE CUDA::cudart)
add_test(NAME test_openpbr_core_cuda COMMAND test_openpbr_core_cuda)
set_tests_properties(test_openpbr_core_cuda PROPERTIES SKIP_RETURN_CODE 77)

add_executable(test_light_sampling_cuda)
target_sources(test_light_sampling_cuda
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_light_sampling_cuda.cu
)
target_include_directories(test_light_sampling_cuda PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_light_sampling_cuda PRIVATE CUDA::cudart)
add_test(NAME test_light_sampling_cuda COMMAND test_light_sampling_cuda)
set_tests_properties(test_light_sampling_cuda PROPERTIES
    LABELS "physical;gpu"
    SKIP_RETURN_CODE 77
)

add_executable(test_device_scene_buffers)
target_sources(test_device_scene_buffers
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_device_scene_buffers.cpp
)
target_link_libraries(test_device_scene_buffers PRIVATE realtime_gpu)
add_test(NAME test_device_scene_buffers COMMAND test_device_scene_buffers)

add_executable(test_device_frame_buffers)
target_sources(test_device_frame_buffers
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_device_frame_buffers.cpp
)
target_link_libraries(test_device_frame_buffers PRIVATE realtime_gpu)
add_test(NAME test_device_frame_buffers COMMAND test_device_frame_buffers)

add_executable(test_host_radiance_staging)
target_sources(test_host_radiance_staging
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_host_radiance_staging.cpp
)
target_link_libraries(test_host_radiance_staging PRIVATE realtime_gpu)
add_test(NAME test_host_radiance_staging COMMAND test_host_radiance_staging)

add_executable(test_cuda_event_timer)
target_sources(test_cuda_event_timer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cuda_event_timer.cpp
)
target_link_libraries(test_cuda_event_timer PRIVATE realtime_gpu)
add_test(NAME test_cuda_event_timer COMMAND test_cuda_event_timer)

add_executable(test_direction_debug_renderer)
target_sources(test_direction_debug_renderer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_direction_debug_renderer.cpp
)
target_link_libraries(test_direction_debug_renderer PRIVATE realtime_gpu)
add_test(NAME test_direction_debug_renderer COMMAND test_direction_debug_renderer)

add_executable(test_render_request_validation)
target_sources(test_render_request_validation
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_render_request_validation.cpp
)
target_link_libraries(test_render_request_validation PRIVATE realtime_gpu)
add_test(NAME test_render_request_validation COMMAND test_render_request_validation)

add_executable(test_radiance_launch_setup)
target_sources(test_radiance_launch_setup
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_radiance_launch_setup.cpp
)
target_link_libraries(test_radiance_launch_setup PRIVATE realtime_gpu)
add_test(NAME test_radiance_launch_setup COMMAND test_radiance_launch_setup)

add_executable(test_radiance_frame_assembly)
target_sources(test_radiance_frame_assembly
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_radiance_frame_assembly.cpp
)
target_link_libraries(test_radiance_frame_assembly PRIVATE realtime_gpu)
add_test(NAME test_radiance_frame_assembly COMMAND test_radiance_frame_assembly)

add_executable(test_optix_path_trace)
target_sources(test_optix_path_trace
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_path_trace.cpp
)
target_link_libraries(test_optix_path_trace PRIVATE realtime_gpu)
add_test(NAME test_optix_path_trace COMMAND test_optix_path_trace)

add_executable(test_openpbr_render_path)
target_sources(test_openpbr_render_path
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_openpbr_render_path.cpp
)
target_link_libraries(test_openpbr_render_path PRIVATE realtime_gpu)
add_test(NAME test_openpbr_render_path COMMAND test_openpbr_render_path)

add_executable(test_openpbr_legacy_compatibility)
target_sources(test_openpbr_legacy_compatibility
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_openpbr_legacy_compatibility.cpp
)
target_link_libraries(test_openpbr_legacy_compatibility PRIVATE realtime_gpu)
add_test(NAME test_openpbr_legacy_compatibility COMMAND test_openpbr_legacy_compatibility)

add_executable(test_optix_profiled_render)
target_sources(test_optix_profiled_render
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_profiled_render.cpp
)
target_link_libraries(test_optix_profiled_render PRIVATE realtime_gpu)
add_test(NAME test_optix_profiled_render COMMAND test_optix_profiled_render)

add_executable(test_optix_temporal_reference)
target_sources(test_optix_temporal_reference
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_temporal_reference.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/four_camera_rig.cpp
)
target_link_libraries(test_optix_temporal_reference PRIVATE realtime_gpu core)
add_test(NAME test_optix_temporal_reference COMMAND test_optix_temporal_reference)

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
set(RENDER_REALTIME_PROVENANCE_HEADER
    ${CMAKE_CURRENT_BINARY_DIR}/include/realtime/build_provenance.h
)
add_custom_target(render_realtime_build_provenance
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DOUTPUT_FILE=${RENDER_REALTIME_PROVENANCE_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildProvenance.cmake
    BYPRODUCTS ${RENDER_REALTIME_PROVENANCE_HEADER}
    COMMENT "Refreshing render_realtime source provenance"
    VERBATIM
)
target_sources(render_realtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/render_realtime.cpp
    ${RENDER_REALTIME_PROVENANCE_HEADER}
)
add_dependencies(render_realtime render_realtime_build_provenance)
target_link_libraries(render_realtime PRIVATE realtime_gpu core)
target_compile_definitions(render_realtime PRIVATE
    RT_BUILD_CONFIGURATION="${RT_BUILD_CONFIGURATION}"
    RT_CXX_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)
add_test(NAME test_render_realtime_cli
    COMMAND $<TARGET_FILE:render_realtime>
        --camera-count 1
        --frames 1
        --warmup-frames 1
        --seed 7
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
