#pragma once

#include "realtime/camera_models.h"

#include <Eigen/Geometry>

#include <array>

namespace rt::scene {

struct Pinhole32Slot {
    double k1 = 0.0;
    double k2 = 0.0;
    double k3 = 0.0;
    double p1 = 0.0;
    double p2 = 0.0;
};

struct Equi62Lut1DSlot {
    std::array<double, 6> radial {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    Eigen::Vector2d tangential = Eigen::Vector2d::Zero();
};

struct CameraSpec {
    CameraModelType model = CameraModelType::pinhole32;
    int width = 0;
    int height = 0;
    double fx = 0.0;
    double fy = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
    Pinhole32Slot pinhole32 {};
    Equi62Lut1DSlot equi62_lut1d {};
};

}  // namespace rt::scene
