#include "realtime/viewer/four_camera_rig.h"

#include "realtime/frame_convention.h"

#include <Eigen/Geometry>

#include <array>
#include <numbers>

namespace rt::viewer {

namespace {

Eigen::Vector3d world_up(rt::viewer::ViewerFrameConvention convention) {
    if (convention == rt::viewer::ViewerFrameConvention::legacy_y_up) {
        return Eigen::Vector3d::UnitY();
    }
    return Eigen::Vector3d::UnitZ();
}

Eigen::Vector3d neutral_right() {
    return Eigen::Vector3d::UnitX();
}

}  // namespace

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height, ViewerFrameConvention convention) {
    CameraRig rig;

    const Pinhole32Params pinhole {
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

    for (const double yaw_offset : rt::kDefaultSurroundYawOffsetsDeg) {
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
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
        T_bc.linear() = camera_to_renderer_matrix().transpose() * R_rc;
        rig.add_pinhole(pinhole, T_bc, width, height);
    }

    return rig;
}

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height) {
    return make_default_viewer_rig(pose, width, height, default_viewer_frame_convention());
}

}  // namespace rt::viewer
