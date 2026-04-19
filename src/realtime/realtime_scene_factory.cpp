#include "realtime/realtime_scene_factory.h"

#include "realtime/default_viewer_conventions.h"
#include "realtime/frame_convention.h"
#include "realtime/scene_catalog.h"
#include "scene/realtime_scene_adapter.h"
#include "scene/shared_scene_builders.h"

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <cmath>
#include <stdexcept>

namespace rt {
namespace {

constexpr double kPi = 3.14159265358979323846;

Pinhole32Params make_pinhole_from_vfov(double vfov_deg, int width, int height) {
    const double theta = vfov_deg * kPi / 180.0;
    const double fy = 0.5 * static_cast<double>(height) / std::tan(theta * 0.5);
    return Pinhole32Params {
        .fx = fy,
        .fy = fy,
        .cx = 0.5 * static_cast<double>(width),
        .cy = 0.5 * static_cast<double>(height),
        .k1 = 0.0,
        .k2 = 0.0,
        .k3 = 0.0,
        .p1 = 0.0,
        .p2 = 0.0,
    };
}

Pinhole32Params make_default_viewer_pinhole(int width, int height) {
    return Pinhole32Params {
        .fx = 0.75 * static_cast<double>(width),
        .fy = 0.75 * static_cast<double>(height),
        .cx = 0.5 * static_cast<double>(width),
        .cy = 0.5 * static_cast<double>(height),
        .k1 = 0.0,
        .k2 = 0.0,
        .k3 = 0.0,
        .p1 = 0.0,
        .p2 = 0.0,
    };
}

CameraRig make_camera_rig_from_preset(const scene::RealtimeViewPreset& preset, int camera_count, int width, int height) {
    CameraRig rig;
    const Pinhole32Params pinhole = preset.use_default_viewer_intrinsics
        ? make_default_viewer_pinhole(width, height)
        : make_pinhole_from_vfov(preset.vfov_deg, width, height);

    for (int i = 0; i < camera_count; ++i) {
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
        T_bc.translation() = body_to_renderer_matrix().transpose() * preset.initial_body_pose.position;
        viewer::BodyPose camera_pose = preset.initial_body_pose;
        camera_pose.yaw_deg += kDefaultSurroundYawOffsetsDeg[static_cast<std::size_t>(i)];

        const Eigen::Vector3d forward = viewer::forward_direction(camera_pose, preset.frame_convention);
        Eigen::Vector3d right = viewer::right_direction(camera_pose, preset.frame_convention);
        if (right.squaredNorm() < 1e-12) {
            right = Eigen::Vector3d::UnitX();
        }
        const Eigen::Vector3d up = right.cross(forward).normalized();

        Eigen::Matrix3d R_rc = Eigen::Matrix3d::Identity();
        R_rc.col(0) = right;
        R_rc.col(1) = -up;
        R_rc.col(2) = forward;
        T_bc.linear() = camera_to_renderer_matrix().transpose() * R_rc;
        rig.add_pinhole(pinhole, T_bc, width, height);
    }

    return rig;
}

const scene::RealtimeViewPreset& require_realtime_view_preset(std::string_view scene_id) {
    const scene::RealtimeViewPreset* preset = scene::find_realtime_view_preset(scene_id);
    if (preset == nullptr) {
        throw std::invalid_argument("unsupported realtime scene");
    }
    return *preset;
}

}  // namespace

bool realtime_scene_supported(std::string_view scene_id) {
    const SceneCatalogEntry* entry = find_scene_catalog_entry(scene_id);
    return entry != nullptr && entry->supports_realtime;
}

SceneDescription make_realtime_scene(std::string_view scene_id) {
    if (!realtime_scene_supported(scene_id)) {
        throw std::invalid_argument("unsupported realtime scene");
    }
    return scene::adapt_to_realtime(scene::build_scene(scene_id));
}

viewer::ViewerFrameConvention viewer_frame_convention_for_scene(std::string_view scene_id) {
    return require_realtime_view_preset(scene_id).frame_convention;
}

CameraRig default_camera_rig_for_scene(std::string_view scene_id, int camera_count, int width, int height) {
    if (!realtime_scene_supported(scene_id)) {
        throw std::invalid_argument("unsupported realtime scene");
    }
    if (camera_count < 1 || camera_count > 4) {
        throw std::invalid_argument("camera_count must be in [1, 4]");
    }
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("camera rig dimensions must be positive");
    }
    return make_camera_rig_from_preset(require_realtime_view_preset(scene_id), camera_count, width, height);
}

viewer::BodyPose default_spawn_pose_for_scene(std::string_view scene_id) {
    return require_realtime_view_preset(scene_id).initial_body_pose;
}

double default_move_speed_for_scene(std::string_view scene_id) {
    return require_realtime_view_preset(scene_id).base_move_speed;
}

}  // namespace rt
