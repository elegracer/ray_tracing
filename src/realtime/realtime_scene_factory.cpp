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

scene::CameraSpec runtime_camera_spec(
    const scene::CameraSpec& authored, int width, int height, const Eigen::Isometry3d& T_bc) {
    if (authored.width <= 0 || authored.height <= 0) {
        throw std::invalid_argument("authored camera dimensions must be positive");
    }

    scene::CameraSpec runtime = authored;
    const double scale_x = static_cast<double>(width) / static_cast<double>(authored.width);
    const double scale_y = static_cast<double>(height) / static_cast<double>(authored.height);
    runtime.width = width;
    runtime.height = height;
    runtime.fx = authored.fx * scale_x;
    runtime.fy = authored.fy * scale_y;
    runtime.cx = authored.cx * scale_x;
    runtime.cy = authored.cy * scale_y;
    runtime.T_bc = T_bc * authored.T_bc;
    return runtime;
}

CameraRig make_camera_rig_from_preset(const scene::RealtimeViewPreset& preset, int camera_count, int width, int height) {
    CameraRig rig;

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
        rig.add_camera(runtime_camera_spec(preset.camera, width, height, T_bc));
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
    SceneDescription out = scene::adapt_to_realtime(scene::build_scene(scene_id));
    out.background = scene::scene_background(scene_id);
    return out;
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
