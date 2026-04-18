#include "realtime/viewer/four_camera_rig.h"

#include "realtime/frame_convention.h"

#include <Eigen/Geometry>

#include <array>
#include <numbers>

namespace rt::viewer {

namespace {

double to_radians(double degrees) {
    return degrees * std::numbers::pi / 180.0;
}

Eigen::Matrix3d body_rotation_matrix(double yaw_deg, double pitch_deg) {
    const Eigen::AngleAxisd yaw(to_radians(yaw_deg), Eigen::Vector3d::UnitX());
    const Eigen::AngleAxisd pitch(to_radians(-pitch_deg), Eigen::Vector3d::UnitY());
    return (yaw * pitch).toRotationMatrix();
}

Eigen::Matrix3d body_yaw_offset_matrix(double yaw_deg) {
    return Eigen::AngleAxisd(to_radians(yaw_deg), Eigen::Vector3d::UnitX()).toRotationMatrix();
}

}  // namespace

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height) {
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
        T_bc.linear() = front_camera_to_body_matrix().transpose()
            * body_rotation_matrix(pose.yaw_deg, pose.pitch_deg)
            * body_yaw_offset_matrix(yaw_offset)
            * front_camera_to_body_matrix();
        rig.add_pinhole(pinhole, T_bc, width, height);
    }

    return rig;
}

}  // namespace rt::viewer
