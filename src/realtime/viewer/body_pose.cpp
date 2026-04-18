#include "realtime/viewer/body_pose.h"

#include "realtime/frame_convention.h"

#include <Eigen/Geometry>

#include <algorithm>
#include <numbers>

namespace rt::viewer {

namespace {

double to_radians(double degrees) {
    return degrees * std::numbers::pi / 180.0;
}

Eigen::Matrix3d body_rotation_matrix(const BodyPose& pose) {
    const Eigen::AngleAxisd yaw(to_radians(pose.yaw_deg), Eigen::Vector3d::UnitX());
    const Eigen::AngleAxisd pitch(to_radians(-pose.pitch_deg), Eigen::Vector3d::UnitY());
    return (yaw * pitch).toRotationMatrix();
}

Eigen::Matrix3d body_to_world_rotation(const BodyPose& pose) {
    return body_to_world_matrix() * body_rotation_matrix(pose);
}

} // namespace

BodyPose default_spawn_pose() {
    return BodyPose {.position = Eigen::Vector3d(0.0, -0.8, 0.35), .yaw_deg = 0.0, .pitch_deg = 0.0};
}

double clamp_pitch_deg(double pitch_deg) {
    return std::clamp(pitch_deg, -80.0, 80.0);
}

Eigen::Vector3d forward_direction(const BodyPose& pose) {
    return (body_to_world_rotation(pose) * Eigen::Vector3d(0.0, 0.0, -1.0)).normalized();
}

Eigen::Vector3d right_direction(const BodyPose& pose) {
    return (body_to_world_rotation(pose) * Eigen::Vector3d(0.0, -1.0, 0.0)).normalized();
}

void integrate_mouse_look(BodyPose& pose, double delta_x, double delta_y, double degrees_per_pixel) {
    pose.yaw_deg += delta_x * degrees_per_pixel;
    pose.pitch_deg = clamp_pitch_deg(pose.pitch_deg - delta_y * degrees_per_pixel);
}

void integrate_wasd(BodyPose& pose, bool move_forward, bool move_backward, bool move_left, bool move_right,
    bool move_down, bool move_up, double distance) {
    Eigen::Vector3d local_delta = Eigen::Vector3d::Zero();
    if (move_forward) {
        local_delta += Eigen::Vector3d(0.0, 0.0, -1.0);
    }
    if (move_backward) {
        local_delta += Eigen::Vector3d(0.0, 0.0, 1.0);
    }
    if (move_left) {
        local_delta += Eigen::Vector3d(0.0, 1.0, 0.0);
    }
    if (move_right) {
        local_delta += Eigen::Vector3d(0.0, -1.0, 0.0);
    }
    if (move_down) {
        local_delta += Eigen::Vector3d(-1.0, 0.0, 0.0);
    }
    if (move_up) {
        local_delta += Eigen::Vector3d(1.0, 0.0, 0.0);
    }

    if (local_delta.squaredNorm() > 0.0) {
        const Eigen::Vector3d world_delta = body_to_world_rotation(pose) * local_delta.normalized();
        pose.position += world_delta * distance;
    }
}

} // namespace rt::viewer
