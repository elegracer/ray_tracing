#include "realtime/camera_models.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace rt {

namespace {

constexpr double kEpsilon = 1e-12;

Eigen::Vector2d distort_pinhole_normalized(const Pinhole32Params& params, const Eigen::Vector2d& xy) {
    const double x = xy.x();
    const double y = xy.y();
    const double x2 = x * x;
    const double y2 = y * y;
    const double xy_term = x * y;
    const double r2 = x2 + y2;
    const double r4 = r2 * r2;
    const double r6 = r4 * r2;
    const double radial = 1.0 + params.k1 * r2 + params.k2 * r4 + params.k3 * r6;

    return Eigen::Vector2d {
        x * radial + params.p2 * (r2 + 2.0 * x2) + 2.0 * params.p1 * xy_term,
        y * radial + params.p1 * (r2 + 2.0 * y2) + 2.0 * params.p2 * xy_term,
    };
}

Eigen::Matrix2d pinhole_distortion_jacobian(const Pinhole32Params& params, const Eigen::Vector2d& xy) {
    const double x = xy.x();
    const double y = xy.y();
    const double r2 = x * x + y * y;
    const double r4 = r2 * r2;
    const double r6 = r4 * r2;
    const double radial = 1.0 + params.k1 * r2 + params.k2 * r4 + params.k3 * r6;
    const double d_radial_dx = 2.0 * x * (params.k1 + 2.0 * params.k2 * r2 + 3.0 * params.k3 * r4);
    const double d_radial_dy = 2.0 * y * (params.k1 + 2.0 * params.k2 * r2 + 3.0 * params.k3 * r4);

    Eigen::Matrix2d jacobian;
    jacobian(0, 0) = radial + x * d_radial_dx + 6.0 * params.p2 * x + 2.0 * params.p1 * y;
    jacobian(0, 1) = x * d_radial_dy + 2.0 * params.p1 * x + 2.0 * params.p2 * y;
    jacobian(1, 0) = y * d_radial_dx + 2.0 * params.p1 * x + 2.0 * params.p2 * y;
    jacobian(1, 1) = radial + y * d_radial_dy + 6.0 * params.p1 * y + 2.0 * params.p2 * x;
    return jacobian;
}

bool solve_2x2(const Eigen::Matrix2d& jacobian, const Eigen::Vector2d& rhs, Eigen::Vector2d& delta) {
    const double det = jacobian(0, 0) * jacobian(1, 1) - jacobian(0, 1) * jacobian(1, 0);
    if (std::abs(det) < kEpsilon) {
        return false;
    }

    const double inv_det = 1.0 / det;
    delta.x() = (jacobian(1, 1) * rhs.x() - jacobian(0, 1) * rhs.y()) * inv_det;
    delta.y() = (-jacobian(1, 0) * rhs.x() + jacobian(0, 0) * rhs.y()) * inv_det;
    return true;
}

double eval_equi_theta_distortion(const std::array<double, 6>& radial, double theta) {
    const double theta2 = theta * theta;
    double scale = 1.0;
    double theta_pow = theta2;
    for (double coeff : radial) {
        scale += coeff * theta_pow;
        theta_pow *= theta2;
    }
    return theta * scale;
}

double eval_equi_theta_derivative(const std::array<double, 6>& radial, double theta) {
    const double theta2 = theta * theta;
    double derivative = 1.0;
    double theta_pow = theta2;
    for (std::size_t i = 0; i < radial.size(); ++i) {
        derivative += static_cast<double>(2 * i + 3) * radial[i] * theta_pow;
        theta_pow *= theta2;
    }
    return derivative;
}

double invert_equi_theta_distortion(const std::array<double, 6>& radial, double thetad) {
    double theta = std::cbrt(std::max(thetad, 0.0));
    for (int iter = 0; iter < 30; ++iter) {
        const double error = thetad - eval_equi_theta_distortion(radial, theta);
        if (std::abs(error) < kEpsilon) {
            break;
        }

        const double derivative = eval_equi_theta_derivative(radial, theta);
        if (std::abs(derivative) < kEpsilon) {
            break;
        }

        const double delta = error / derivative;
        theta += delta;
        if (std::abs(delta) < kEpsilon || std::abs(delta) > 3.1416) {
            break;
        }
    }
    return theta;
}

Eigen::Vector2d apply_equi_tangential(const Eigen::Vector2d& xy, const Eigen::Vector2d& tangential) {
    const double xr = xy.x();
    const double yr = xy.y();
    const double xr2 = xr * xr;
    const double yr2 = yr * yr;
    const double xryr = xr * yr;
    const double p1 = tangential.x();
    const double p2 = tangential.y();

    return Eigen::Vector2d {
        xr + 2.0 * p1 * xryr + p2 * (xr2 + yr2 + 2.0 * xr2),
        yr + p1 * (xr2 + yr2 + 2.0 * yr2) + 2.0 * p2 * xryr,
    };
}

Eigen::Matrix2d equi_tangential_jacobian(const Eigen::Vector2d& xy, const Eigen::Vector2d& tangential) {
    const double xr = xy.x();
    const double yr = xy.y();
    const double p1 = tangential.x();
    const double p2 = tangential.y();

    Eigen::Matrix2d jacobian;
    jacobian(0, 0) = 1.0 + 2.0 * p1 * yr + 6.0 * p2 * xr;
    jacobian(0, 1) = 2.0 * p1 * xr + 2.0 * p2 * yr;
    jacobian(1, 0) = jacobian(0, 1);
    jacobian(1, 1) = 1.0 + 6.0 * p1 * yr + 2.0 * p2 * xr;
    return jacobian;
}

Eigen::Vector2d remove_equi_tangential_single_step(
    const Eigen::Vector2d& xy_distorted, const Eigen::Vector2d& tangential) {
    Eigen::Vector2d xy = xy_distorted;
    const Eigen::Vector2d distorted = apply_equi_tangential(xy, tangential);
    const Eigen::Matrix2d jacobian = equi_tangential_jacobian(xy, tangential);
    Eigen::Vector2d delta;
    if (!solve_2x2(jacobian, xy_distorted - distorted, delta)) {
        return xy;
    }

    xy += delta;
    return xy;
}

bool interpolate_lut_theta(const Equi62Lut1DParams& params, double rd, double& theta) {
    if (rd <= 0.0 || params.lut_step <= 0.0) {
        theta = 0.0;
        return true;
    }

    const double position = rd / params.lut_step;
    const double max_index = static_cast<double>(params.lut.size() - 1);
    if (position > max_index) {
        return false;
    }
    if (position >= max_index) {
        theta = params.lut.back();
        return true;
    }

    const std::size_t index = static_cast<std::size_t>(position);
    const double alpha = position - static_cast<double>(index);
    theta = (1.0 - alpha) * params.lut[index] + alpha * params.lut[index + 1];
    return true;
}

Eigen::Vector3d normalized_fallback_ray(const Eigen::Vector2d& xy) {
    return Eigen::Vector3d {xy.x(), xy.y(), 1.0}.normalized();
}

}  // namespace

Equi62Lut1DParams make_equi62_lut1d_params(int width, int height, double fx, double fy,
    double cx, double cy, const std::array<double, 6>& radial, const Eigen::Vector2d& tangential) {
    Equi62Lut1DParams params {};
    params.width = width;
    params.height = height;
    params.fx = fx;
    params.fy = fy;
    params.cx = cx;
    params.cy = cy;
    params.radial = radial;
    params.tangential = tangential;

    const double x = (static_cast<double>(width) + 10.0 - cx) / fx;
    const double y = (static_cast<double>(height) + 10.0 - cy) / fy;
    params.lut_step = std::sqrt(x * x + y * y) / static_cast<double>(params.lut.size());
    for (std::size_t i = 0; i < params.lut.size(); ++i) {
        params.lut[i] = invert_equi_theta_distortion(params.radial, params.lut_step * static_cast<double>(i));
    }

    return params;
}

Eigen::Vector2d project_pinhole32(const Pinhole32Params& params, const Eigen::Vector3d& dir_cam) {
    const Eigen::Vector2d xy(dir_cam.x() / dir_cam.z(), dir_cam.y() / dir_cam.z());
    const Eigen::Vector2d xy_distorted = distort_pinhole_normalized(params, xy);
    return Eigen::Vector2d {
        params.fx * xy_distorted.x() + params.cx,
        params.fy * xy_distorted.y() + params.cy,
    };
}

Eigen::Vector3d unproject_pinhole32(const Pinhole32Params& params, const Eigen::Vector2d& pixel) {
    const Eigen::Vector2d xy_distorted(
        (pixel.x() - params.cx) / params.fx,
        (pixel.y() - params.cy) / params.fy);

    Eigen::Vector2d xy = xy_distorted;
    for (int iter = 0; iter < 8; ++iter) {
        const Eigen::Vector2d error = xy_distorted - distort_pinhole_normalized(params, xy);
        if (error.squaredNorm() < kEpsilon * kEpsilon) {
            break;
        }

        const Eigen::Matrix2d jacobian = pinhole_distortion_jacobian(params, xy);
        Eigen::Vector2d delta;
        if (!solve_2x2(jacobian, error, delta)) {
            break;
        }

        xy += delta;
    }

    return Eigen::Vector3d {xy.x(), xy.y(), 1.0}.normalized();
}

Eigen::Vector2d project_equi62_lut1d(const Equi62Lut1DParams& params, const Eigen::Vector3d& dir_cam) {
    const Eigen::Vector2d uv_norm(dir_cam.x() / dir_cam.z(), dir_cam.y() / dir_cam.z());
    const double r = uv_norm.norm();
    const double theta = std::atan(r);
    const double thetad = eval_equi_theta_distortion(params.radial, theta);
    const double cdist = r > kEpsilon ? thetad / r : 1.0;
    const Eigen::Vector2d xy_radial = uv_norm * cdist;
    const Eigen::Vector2d xy_distorted = apply_equi_tangential(xy_radial, params.tangential);

    return Eigen::Vector2d {
        params.fx * xy_distorted.x() + params.cx,
        params.fy * xy_distorted.y() + params.cy,
    };
}

Eigen::Vector3d unproject_equi62_lut1d(const Equi62Lut1DParams& params, const Eigen::Vector2d& pixel) {
    const Eigen::Vector2d xy(
        (pixel.x() - params.cx) / params.fx,
        (pixel.y() - params.cy) / params.fy);
    if (xy.squaredNorm() < kEpsilon * kEpsilon) {
        return Eigen::Vector3d {0.0, 0.0, 1.0};
    }

    const Eigen::Vector2d xy_radial = remove_equi_tangential_single_step(xy, params.tangential);
    const double rd = xy_radial.norm();
    if (rd < kEpsilon) {
        return Eigen::Vector3d {0.0, 0.0, 1.0};
    }

    double theta = 0.0;
    if (!interpolate_lut_theta(params, rd, theta)) {
        return normalized_fallback_ray(xy);
    }

    const double scale = std::tan(theta) / rd;
    return Eigen::Vector3d {xy_radial.x() * scale, xy_radial.y() * scale, 1.0}.normalized();
}

}  // namespace rt
