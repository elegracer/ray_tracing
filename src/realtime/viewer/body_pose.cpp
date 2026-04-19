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

Eigen::Vector3d world_up(ViewerFrameConvention convention) {
    if (convention == ViewerFrameConvention::legacy_y_up) {
        return Eigen::Vector3d::UnitY();
    }
    return Eigen::Vector3d::UnitZ();
}

Eigen::Vector3d neutral_forward(ViewerFrameConvention convention) {
    if (convention == ViewerFrameConvention::legacy_y_up) {
        return Eigen::Vector3d(0.0, 0.0, -1.0);
    }
    return Eigen::Vector3d(0.0, 1.0, 0.0);
}

Eigen::Vector3d neutral_right() {
    return Eigen::Vector3d::UnitX();
}

Eigen::Matrix3d body_basis_to_world(ViewerFrameConvention convention) {
    Eigen::Matrix3d basis = Eigen::Matrix3d::Identity();
    basis.col(0) = world_up(convention);
    basis.col(1) = -neutral_right();
    basis.col(2) = -neutral_forward(convention);
    return basis;
}

Eigen::Matrix3d body_rotation_matrix(const BodyPose& pose) {
    const Eigen::AngleAxisd yaw(to_radians(pose.yaw_deg), Eigen::Vector3d::UnitX());
    const Eigen::AngleAxisd pitch(to_radians(-pose.pitch_deg), Eigen::Vector3d::UnitY());
    return (yaw * pitch).toRotationMatrix();
}

Eigen::Matrix3d body_to_world_rotation(const BodyPose& pose, ViewerFrameConvention convention) {
    return body_basis_to_world(convention) * body_rotation_matrix(pose);
}

} // namespace

ViewerFrameConvention default_viewer_frame_convention() {
    return ViewerFrameConvention::world_z_up;
}

BodyPose default_spawn_pose() {
    return BodyPose {.position = Eigen::Vector3d(0.0, -0.8, 0.35), .yaw_deg = 0.0, .pitch_deg = 0.0};
}

double clamp_pitch_deg(double pitch_deg) {
    return std::clamp(pitch_deg, -80.0, 80.0);
}

Eigen::Vector3d forward_direction(const BodyPose& pose, ViewerFrameConvention convention) {
    return (body_to_world_rotation(pose, convention) * Eigen::Vector3d(0.0, 0.0, -1.0)).normalized();
}

Eigen::Vector3d forward_direction(const BodyPose& pose) {
    return forward_direction(pose, default_viewer_frame_convention());
}

Eigen::Vector3d right_direction(const BodyPose& pose, ViewerFrameConvention convention) {
    Eigen::Vector3d right = forward_direction(pose, convention).cross(world_up(convention));
    if (right.squaredNorm() < 1e-12) {
        if (convention == ViewerFrameConvention::legacy_y_up) {
            return neutral_right();
        }
        return (body_to_world_rotation(pose, convention) * Eigen::Vector3d(0.0, -1.0, 0.0)).normalized();
    }
    return right.normalized();
}

Eigen::Vector3d right_direction(const BodyPose& pose) {
    return right_direction(pose, default_viewer_frame_convention());
}

void integrate_mouse_look(BodyPose& pose, double delta_x, double delta_y, double degrees_per_pixel) {
    pose.yaw_deg -= delta_x * degrees_per_pixel;
    pose.pitch_deg = clamp_pitch_deg(pose.pitch_deg - delta_y * degrees_per_pixel);
}

void integrate_wasd(BodyPose& pose, bool move_forward, bool move_backward, bool move_left, bool move_right,
    bool move_down, bool move_up, double distance, ViewerFrameConvention convention) {
    Eigen::Vector3d world_delta = Eigen::Vector3d::Zero();
    const Eigen::Vector3d forward = forward_direction(pose, convention);
    const Eigen::Vector3d right = right_direction(pose, convention);
    const Eigen::Vector3d up = world_up(convention);
    if (move_forward) {
        world_delta += forward;
    }
    if (move_backward) {
        world_delta -= forward;
    }
    if (move_left) {
        world_delta -= right;
    }
    if (move_right) {
        world_delta += right;
    }
    if (move_down) {
        world_delta -= up;
    }
    if (move_up) {
        world_delta += up;
    }

    if (world_delta.squaredNorm() > 0.0) {
        pose.position += world_delta.normalized() * distance;
    }
}

void integrate_wasd(BodyPose& pose, bool move_forward, bool move_backward, bool move_left, bool move_right,
    bool move_down, bool move_up, double distance) {
    integrate_wasd(
        pose, move_forward, move_backward, move_left, move_right, move_down, move_up, distance,
        default_viewer_frame_convention());
}

} // namespace rt::viewer
