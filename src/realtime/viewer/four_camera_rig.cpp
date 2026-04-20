#include "realtime/viewer/four_camera_rig.h"

#include "realtime/frame_convention.h"

#include <Eigen/Geometry>

#include <array>
#include <stdexcept>
#include <vector>

namespace rt::viewer {

namespace {

Eigen::Vector3d neutral_right() {
    return Eigen::Vector3d::UnitX();
}

scene::CameraSpec resize_runtime_camera_spec(
    const scene::CameraSpec& authored, int width, int height, const Sophus::SE3d& T_bc) {
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

scene::CameraSpec default_pinhole_viewer_camera_spec(int width, int height) {
    scene::CameraSpec spec {};
    spec.model = CameraModelType::pinhole32;
    spec.width = width;
    spec.height = height;
    spec.fx = 0.75 * static_cast<double>(width);
    spec.fy = 0.75 * static_cast<double>(height);
    spec.cx = 0.5 * static_cast<double>(width);
    spec.cy = 0.5 * static_cast<double>(height);
    return spec;
}

Sophus::SE3d viewer_pose_transform(
    const BodyPose& pose, double yaw_offset, ViewerFrameConvention convention) {
    Sophus::SE3d T_bc {};
    T_bc.translation() = body_to_world_matrix().transpose() * pose.position;
    BodyPose camera_pose = pose;
    camera_pose.yaw_deg += yaw_offset;
    const Eigen::Vector3d forward = forward_direction(camera_pose, convention);
    Eigen::Vector3d right = right_direction(camera_pose, convention);
    if (right.squaredNorm() < 1e-12) {
        right = neutral_right();
    }
    const Eigen::Vector3d up = right.cross(forward).normalized();
    Eigen::Matrix3d R_rc = Eigen::Matrix3d::Identity();
    R_rc.col(0) = right;
    R_rc.col(1) = -up;
    R_rc.col(2) = forward;
    T_bc.so3() = Sophus::SO3d(camera_to_renderer_matrix().transpose() * R_rc);
    return T_bc;
}

}  // namespace

CameraRig make_default_viewer_rig(const BodyPose& pose, std::span<const scene::CameraSpec> camera_specs, int width,
    int height, ViewerFrameConvention convention) {
    if (camera_specs.empty() || camera_specs.size() > rt::kDefaultSurroundYawOffsetsDeg.size()) {
        throw std::invalid_argument("viewer rig requires between 1 and 4 camera specs");
    }
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("viewer rig dimensions must be positive");
    }

    CameraRig rig;
    for (std::size_t i = 0; i < camera_specs.size(); ++i) {
        const Sophus::SE3d T_bc =
            viewer_pose_transform(pose, rt::kDefaultSurroundYawOffsetsDeg[i], convention);
        rig.add_camera(resize_runtime_camera_spec(camera_specs[i], width, height, T_bc));
    }

    return rig;
}

CameraRig make_default_viewer_rig(const BodyPose& pose, const scene::CameraSpec& camera, int camera_count, int width,
    int height, ViewerFrameConvention convention) {
    if (camera_count < 1 || camera_count > static_cast<int>(rt::kDefaultSurroundYawOffsetsDeg.size())) {
        throw std::invalid_argument("camera_count must be in [1, 4]");
    }
    std::vector<scene::CameraSpec> cameras(static_cast<std::size_t>(camera_count), camera);
    return make_default_viewer_rig(pose, std::span<const scene::CameraSpec>(cameras), width, height, convention);
}

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height, ViewerFrameConvention convention) {
    return make_default_viewer_rig(
        pose, default_pinhole_viewer_camera_spec(width, height), 4, width, height, convention);
}

CameraRig make_default_viewer_rig(
    const BodyPose& pose, std::span<const scene::CameraSpec> camera_specs, int width, int height) {
    return make_default_viewer_rig(pose, camera_specs, width, height, default_viewer_frame_convention());
}

CameraRig make_default_viewer_rig(
    const BodyPose& pose, const scene::CameraSpec& camera, int camera_count, int width, int height) {
    return make_default_viewer_rig(pose, camera, camera_count, width, height, default_viewer_frame_convention());
}

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height) {
    return make_default_viewer_rig(pose, width, height, default_viewer_frame_convention());
}

}  // namespace rt::viewer
