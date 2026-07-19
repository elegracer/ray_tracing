#include "scene/scene_definition.h"

#include "realtime/frame_convention.h"

#include <Eigen/Geometry>
#include <fmt/format.h>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace rt::scene {
namespace {

constexpr double kDefaultHorizontalAperture = 20.955;

Eigen::Matrix4d affine_matrix(const Sophus::SE3d& transform) {
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    matrix.block<3, 3>(0, 0) = transform.rotationMatrix();
    matrix.block<3, 1>(0, 3) = transform.translation();
    return matrix;
}

SceneCameraCalibration compile_calibration(const CameraSpec& spec) {
    SceneCameraCalibration calibration;
    calibration.model = spec.model == CameraModelType::pinhole32
                            ? SceneCameraCalibrationModel::pinhole32
                            : SceneCameraCalibrationModel::equi62_lut1d;
    calibration.image_width = spec.width;
    calibration.image_height = spec.height;
    calibration.focal_length_pixels = Eigen::Vector2d {spec.fx, spec.fy};
    calibration.principal_point_pixels = Eigen::Vector2d {spec.cx, spec.cy};
    calibration.camera_to_body = affine_matrix(spec.T_bc);
    if (spec.model == CameraModelType::pinhole32) {
        calibration.radial_distortion = {
            spec.pinhole32.k1,
            spec.pinhole32.k2,
            spec.pinhole32.k3,
        };
        calibration.tangential_distortion = Eigen::Vector2d {spec.pinhole32.p1, spec.pinhole32.p2};
    } else {
        calibration.radial_distortion.assign(spec.equi62_lut1d.radial.begin(),
            spec.equi62_lut1d.radial.end());
        calibration.tangential_distortion = spec.equi62_lut1d.tangential;
    }
    return calibration;
}

SceneCamera compile_camera(const CameraSpec& spec, double focus_distance = 0.0,
    double defocus_angle_deg = 0.0) {
    SceneCamera camera;
    camera.horizontal_aperture = kDefaultHorizontalAperture;
    if (spec.width > 0 && spec.height > 0 && spec.fx > 0.0 && spec.fy > 0.0) {
        camera.focal_length =
            camera.horizontal_aperture * spec.fx / static_cast<double>(spec.width);
        camera.vertical_aperture = camera.focal_length * static_cast<double>(spec.height) / spec.fy;
        camera.horizontal_aperture_offset =
            (spec.cx - 0.5 * static_cast<double>(spec.width)) * camera.focal_length / spec.fx;
        camera.vertical_aperture_offset =
            (0.5 * static_cast<double>(spec.height) - spec.cy) * camera.focal_length / spec.fy;
    }
    camera.focus_distance = focus_distance;
    if (defocus_angle_deg > 0.0 && focus_distance > 0.0) {
        const double aperture_diameter =
            2.0 * focus_distance * std::tan(0.5 * defocus_angle_deg * std::numbers::pi / 180.0);
        if (aperture_diameter > 0.0) {
            camera.f_stop = 0.1 * camera.focal_length / aperture_diameter;
        }
    }
    camera.renderer_calibration = compile_calibration(spec);
    return camera;
}

Eigen::Matrix4d usd_camera_from_renderer_pose(const Eigen::Matrix3d& renderer_camera_to_world,
    const Eigen::Vector3d& position) {
    // Renderer cameras use +Z forward and +Y down; OpenUSD uses -Z forward and +Y up.
    Eigen::Matrix3d renderer_to_usd_camera = Eigen::Matrix3d::Identity();
    renderer_to_usd_camera(1, 1) = -1.0;
    renderer_to_usd_camera(2, 2) = -1.0;
    Eigen::Matrix4d transform = Eigen::Matrix4d::Identity();
    transform.block<3, 3>(0, 0) = renderer_camera_to_world * renderer_to_usd_camera;
    transform.block<3, 1>(0, 3) = position;
    return transform;
}

Eigen::Matrix4d compile_cpu_camera_transform(const CpuCameraPreset& preset) {
    const Eigen::Vector3d backward = preset.lookfrom - preset.lookat;
    if (!backward.allFinite() || !preset.vup.allFinite()) {
        throw std::invalid_argument("CPU camera look-at vectors must be finite");
    }
    if (backward.squaredNorm() <= 1e-20) {
        const Eigen::Matrix3d renderer_camera_to_world =
            camera_to_renderer_matrix() * preset.camera.T_bc.rotationMatrix();
        const Eigen::Vector3d position =
            body_to_renderer_matrix() * preset.camera.T_bc.translation();
        return usd_camera_from_renderer_pose(renderer_camera_to_world, position);
    }
    const Eigen::Vector3d w = backward.normalized();
    const Eigen::Vector3d right = preset.vup.cross(w);
    if (right.squaredNorm() <= 1e-20) {
        throw std::invalid_argument(
            "CPU camera up vector must not be parallel to its view direction");
    }
    const Eigen::Vector3d u = right.normalized();
    const Eigen::Vector3d v = w.cross(u).normalized();
    Eigen::Matrix3d renderer_camera_to_world;
    renderer_camera_to_world.col(0) = u;
    renderer_camera_to_world.col(1) = -v;
    renderer_camera_to_world.col(2) = -w;
    return usd_camera_from_renderer_pose(renderer_camera_to_world, preset.lookfrom);
}

Eigen::Matrix4d compile_realtime_camera_transform(const RealtimeViewPreset& preset) {
    const Eigen::Vector3d forward =
        viewer::forward_direction(preset.initial_body_pose, preset.frame_convention);
    const Eigen::Vector3d right =
        viewer::right_direction(preset.initial_body_pose, preset.frame_convention);
    if (!forward.allFinite() || !right.allFinite() || right.squaredNorm() <= 1e-20) {
        throw std::invalid_argument("realtime camera pose must define finite camera axes");
    }
    const Eigen::Vector3d up = right.cross(forward).normalized();
    Eigen::Matrix3d renderer_camera_to_world;
    renderer_camera_to_world.col(0) = right;
    renderer_camera_to_world.col(1) = -up;
    renderer_camera_to_world.col(2) = forward;

    Sophus::SE3d pose_camera_to_body;
    pose_camera_to_body.translation() =
        body_to_world_matrix().transpose() * preset.initial_body_pose.position;
    pose_camera_to_body.so3() =
        Sophus::SO3d(camera_to_renderer_matrix().transpose() * renderer_camera_to_world);
    const Sophus::SE3d camera_to_body = pose_camera_to_body * preset.camera.T_bc;
    const Eigen::Matrix3d calibrated_camera_to_world =
        camera_to_renderer_matrix() * camera_to_body.rotationMatrix();
    const Eigen::Vector3d calibrated_position =
        body_to_renderer_matrix() * camera_to_body.translation();
    return usd_camera_from_renderer_pose(calibrated_camera_to_world, calibrated_position);
}

} // namespace

SceneIRv2 compile_scene_definition_v2(const SceneDefinition& definition) {
    SceneIRv2 scene = compile_legacy_scene_ir_v2(definition.scene_ir);
    if (definition.cpu_presets.empty() && !definition.realtime_preset) {
        return scene;
    }

    scene.add_prim(ScenePrim {.path = "/World/Cameras"});
    for (std::size_t index = 0; index < definition.cpu_presets.size(); ++index) {
        const SceneDefinitionCpuRenderPreset& preset = definition.cpu_presets[index];
        SceneCamera camera = compile_camera(preset.camera.camera, preset.camera.focus_dist,
            preset.camera.defocus_angle);
        camera.renderer_calibration->compatibility_pose_fallback =
            (preset.camera.lookfrom - preset.camera.lookat).squaredNorm() <= 1e-20;
        scene.add_prim(ScenePrim {
            .path = fmt::format("/World/Cameras/Cpu_{:04d}", index),
            .kind = ScenePrimKind::camera,
            .local_to_parent = compile_cpu_camera_transform(preset.camera),
            .camera = std::move(camera),
            .compatibility_source_index = index,
            .compatibility_source_name = preset.preset_id,
        });
    }
    if (definition.realtime_preset) {
        scene.add_prim(ScenePrim {
            .path = "/World/Cameras/Realtime_0000",
            .kind = ScenePrimKind::camera,
            .local_to_parent = compile_realtime_camera_transform(*definition.realtime_preset),
            .camera = compile_camera(definition.realtime_preset->camera),
            .compatibility_source_index = 0,
            .compatibility_source_name = "realtime",
        });
    }
    require_valid_scene_ir_v2(scene);
    return scene;
}

} // namespace rt::scene
