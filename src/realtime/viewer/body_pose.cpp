#include "realtime/viewer/body_pose.h"

#include <Eigen/Geometry>

#include <algorithm>
#include <numbers>

namespace rt::viewer {

namespace {

double to_radians(double degrees) {
    return degrees * std::numbers::pi / 180.0;
}

Eigen::Quaterniond look_rotation(const BodyPose& pose) {
    const Eigen::AngleAxisd yaw(to_radians(pose.yaw_deg), Eigen::Vector3d::UnitY());
    const Eigen::AngleAxisd pitch(to_radians(pose.pitch_deg), Eigen::Vector3d::UnitX());
    return Eigen::Quaterniond(yaw * pitch);
}

} // namespace

BodyPose default_spawn_pose() {
    return BodyPose {.position = Eigen::Vector3d(0.0, 0.35, 0.8), .yaw_deg = 0.0, .pitch_deg = 0.0};
}

double clamp_pitch_deg(double pitch_deg) {
    return std::clamp(pitch_deg, -80.0, 80.0);
}

Eigen::Vector3d forward_direction(const BodyPose& pose) {
    const Eigen::Vector3d forward = look_rotation(pose) * Eigen::Vector3d(0.0, 0.0, -1.0);
    return forward.normalized();
}

Eigen::Vector3d right_direction(const BodyPose& pose) {
    const Eigen::Vector3d right = look_rotation(pose) * Eigen::Vector3d(1.0, 0.0, 0.0);
    return right.normalized();
}

void integrate_mouse_look(BodyPose& pose, double delta_x, double delta_y, double degrees_per_pixel) {
    pose.yaw_deg += delta_x * degrees_per_pixel;
    pose.pitch_deg = clamp_pitch_deg(pose.pitch_deg - delta_y * degrees_per_pixel);
}

void integrate_wasd(BodyPose& pose, bool move_forward, bool move_backward, bool move_left, bool move_right,
    double distance) {
    Eigen::Vector3d delta = Eigen::Vector3d::Zero();
    if (move_forward) {
        delta += forward_direction(pose);
    }
    if (move_backward) {
        delta -= forward_direction(pose);
    }
    if (move_right) {
        delta += right_direction(pose);
    }
    if (move_left) {
        delta -= right_direction(pose);
    }

    if (delta.squaredNorm() > 0.0) {
        pose.position += delta.normalized() * distance;
    }
}

} // namespace rt::viewer
