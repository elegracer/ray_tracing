#pragma once

#include <Eigen/Core>

namespace rt {

inline Eigen::Matrix3d camera_to_world_matrix() {
    Eigen::Matrix3d m;
    m.col(0) = Eigen::Vector3d {1.0, 0.0, 0.0};
    m.col(1) = Eigen::Vector3d {0.0, 0.0, -1.0};
    m.col(2) = Eigen::Vector3d {0.0, 1.0, 0.0};
    return m;
}

inline Eigen::Matrix3d body_to_world_matrix() {
    Eigen::Matrix3d m;
    m.col(0) = Eigen::Vector3d {0.0, 0.0, 1.0};
    m.col(1) = Eigen::Vector3d {-1.0, 0.0, 0.0};
    m.col(2) = Eigen::Vector3d {0.0, -1.0, 0.0};
    return m;
}

inline Eigen::Matrix3d front_camera_to_body_matrix() {
    return body_to_world_matrix().transpose() * camera_to_world_matrix();
}

inline Eigen::Vector3d camera_to_world(const Eigen::Vector3d& v) {
    return camera_to_world_matrix() * v;
}

inline Eigen::Vector3d body_to_world(const Eigen::Vector3d& v) {
    return body_to_world_matrix() * v;
}

inline Eigen::Vector3d legacy_renderer_to_world(const Eigen::Vector3d& v) {
    return Eigen::Vector3d {v.x(), -v.z(), v.y()};
}

inline Eigen::Matrix3d camera_to_renderer_matrix() {
    return camera_to_world_matrix();
}

inline Eigen::Matrix3d body_to_renderer_matrix() {
    return body_to_world_matrix();
}

inline Eigen::Vector3d camera_to_renderer(const Eigen::Vector3d& v) {
    return camera_to_world(v);
}

inline Eigen::Vector3d body_to_renderer(const Eigen::Vector3d& v) {
    return body_to_world(v);
}

}  // namespace rt
