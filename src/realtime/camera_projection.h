#pragma once

#include "realtime/camera_rig.h"

#include <Eigen/Core>

namespace rt {

Eigen::Vector2d project_camera_pixel(const PackedCamera& camera, const Eigen::Vector3d& dir_camera);
Eigen::Vector3d unproject_camera_pixel(const PackedCamera& camera, const Eigen::Vector2d& pixel);
Eigen::Vector3d world_direction_for_pixel(const PackedCamera& camera, const Eigen::Vector2d& pixel);

}  // namespace rt
