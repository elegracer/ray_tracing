#pragma once

#include "realtime/viewer/body_pose.h"
#include "scene/shared_scene_ir.h"

#include <string_view>
#include <vector>

namespace rt::scene {

struct SceneMetadata {
    std::string_view id;
    std::string_view label;
    bool supports_cpu_render = false;
    bool supports_realtime = false;
};

struct CpuCameraPreset {
    double aspect_ratio = 16.0 / 9.0;
    int image_width = 1280;
    int max_depth = 50;
    double vfov = 20.0;
    Eigen::Vector3d lookfrom = Eigen::Vector3d::Zero();
    Eigen::Vector3d lookat = Eigen::Vector3d::Zero();
    Eigen::Vector3d vup = Eigen::Vector3d::UnitY();
    Eigen::Vector3d background = Eigen::Vector3d(0.70, 0.80, 1.00);
    double defocus_angle = 0.0;
    double focus_dist = 10.0;
};

struct CpuRenderPreset {
    std::string_view scene_id;
    std::string_view preset_id;
    int samples_per_pixel = 500;
    CpuCameraPreset camera {};
};

struct RealtimeViewPreset {
    viewer::BodyPose initial_body_pose {};
    viewer::ViewerFrameConvention frame_convention = viewer::ViewerFrameConvention::world_z_up;
    double vfov_deg = 20.0;
    bool use_default_viewer_intrinsics = false;
    double base_move_speed = 1.8;
};

const std::vector<SceneMetadata>& scene_metadata();
const SceneMetadata* find_scene_metadata(std::string_view scene_id);
const CpuRenderPreset* find_cpu_render_preset(std::string_view scene_id, std::string_view preset_id);
const CpuRenderPreset* default_cpu_render_preset(std::string_view scene_id);
const RealtimeViewPreset* find_realtime_view_preset(std::string_view scene_id);

SceneIR build_scene(std::string_view scene_id);

}  // namespace rt::scene
