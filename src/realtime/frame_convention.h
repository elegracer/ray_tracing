#pragma once

#include <Eigen/Core>

namespace rt {

inline Eigen::Matrix3d camera_to_renderer_matrix() {
    Eigen::Matrix3d m;
    m.col(0) = Eigen::Vector3d {1.0, 0.0, 0.0};
    m.col(1) = Eigen::Vector3d {0.0, -1.0, 0.0};
    m.col(2) = Eigen::Vector3d {0.0, 0.0, -1.0};
    return m;
}

inline Eigen::Matrix3d body_to_renderer_matrix() {
    Eigen::Matrix3d m;
    m.col(0) = Eigen::Vector3d {0.0, 1.0, 0.0};
    m.col(1) = Eigen::Vector3d {-1.0, 0.0, 0.0};
    m.col(2) = Eigen::Vector3d {0.0, 0.0, 1.0};
    return m;
}

inline Eigen::Vector3d camera_to_renderer(const Eigen::Vector3d& v) {
    return camera_to_renderer_matrix() * v;
}

inline Eigen::Vector3d body_to_renderer(const Eigen::Vector3d& v) {
    return body_to_renderer_matrix() * v;
}

}  // namespace rt
