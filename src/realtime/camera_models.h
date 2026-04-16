#pragma once

#include <Eigen/Core>

#include <array>

namespace rt {

enum class CameraModelType {
    pinhole32,
    equi62_lut1d,
};

struct Pinhole32Params {
    double fx;
    double fy;
    double cx;
    double cy;
    double k1;
    double k2;
    double k3;
    double p1;
    double p2;
};

struct Equi62Lut1DParams {
    int width;
    int height;
    double fx;
    double fy;
    double cx;
    double cy;
    std::array<double, 6> radial;
    Eigen::Vector2d tangential;
    std::array<double, 1024> lut;
    double lut_step;
};

Equi62Lut1DParams make_equi62_lut1d_params(int width, int height, double fx, double fy,
    double cx, double cy, const std::array<double, 6>& radial, const Eigen::Vector2d& tangential);

Eigen::Vector2d project_pinhole32(const Pinhole32Params& params, const Eigen::Vector3d& dir_cam);
Eigen::Vector3d unproject_pinhole32(const Pinhole32Params& params, const Eigen::Vector2d& pixel);

Eigen::Vector2d project_equi62_lut1d(const Equi62Lut1DParams& params, const Eigen::Vector3d& dir_cam);
Eigen::Vector3d unproject_equi62_lut1d(const Equi62Lut1DParams& params, const Eigen::Vector2d& pixel);

}  // namespace rt
