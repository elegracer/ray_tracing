#pragma once

#include <Eigen/Core>

namespace rt::viewer {

struct BodyPose {
    Eigen::Vector3d position;
    double yaw_deg;
    double pitch_deg;
};

BodyPose default_spawn_pose();

double clamp_pitch_deg(double pitch_deg);

Eigen::Vector3d forward_direction(const BodyPose& pose);

Eigen::Vector3d right_direction(const BodyPose& pose);

void integrate_mouse_look(BodyPose& pose, double delta_x, double delta_y, double degrees_per_pixel);

void integrate_wasd(BodyPose& pose, bool move_forward, bool move_backward, bool move_left, bool move_right,
    double distance);

} // namespace rt::viewer
