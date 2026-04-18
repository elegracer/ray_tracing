#pragma once

#include "cam_base.h"

namespace pico_cam {

class CamPinhole32 final : public CamBase {
   public:
    /**
     * @brief Default constructor
     * @param width Width of the camera (raw pixels)
     * @param height Height of the camera (raw pixels)
     */
    CamPinhole32(int width, int height, cv::Mat mask = cv::Mat()) : CamBase(width, height, mask, CAM_PINHOLE32) {
        m_cameraModel = "pinhole";
        m_distortionModel = "brown32";
    }

    ~CamPinhole32() {
    }

    /**
     * @brief This will set and update the camera calibration values.
     * This should be called on startup for each camera and after update!
     * @param calib Camera calibration information
     */
    void setParam(const Eigen::VectorXd &calib) override {
        // pinhole_camera is (fx & fy & cx & cy & k1 & k2 & k3 & p1 & p2)
        assert(calib.rows() == 9);
        m_cameraParam = calib;
    }
    void setProjParam(const Eigen::VectorXd &proj_param) override {
        assert(proj_param.rows() == 4);
        m_cameraParam.head<4>() = proj_param;
    }
    Eigen::Vector2d getFxfy() const override {
        return m_cameraParam.segment<2>(0);
    }
    Eigen::Vector2d getCxcy() const override {
        return m_cameraParam.segment<2>(2);
    }

    PICO_CAMERA_IMPLEMENTATION();

    Eigen::Vector2d distortNoDistortParam(const Eigen::Vector2d &uv_norm, Eigen::Matrix2d &Jd2u) const override {
        double x = uv_norm[0];
        double y = uv_norm[1];

        Eigen::Vector4d cam_k = m_cameraParam.segment<4>(0);

        // Calculate distorted coordinates for fisheye
        Eigen::Vector2d uv_dist;
        uv_dist(0) = cam_k(0) * x + cam_k(2);
        uv_dist(1) = cam_k(1) * y + cam_k(3);

        Jd2u.setZero();
        Jd2u(0, 0) = cam_k(0);
        Jd2u(1, 1) = cam_k(1);

        return uv_dist;
    }

    Eigen::Vector2d undistortNoDistortParam(const Eigen::Vector2d &uv_dist) const override {
        // fx, fy, cx, cy
        Eigen::Vector4d cam_k = m_cameraParam.segment<4>(0);
        Eigen::Vector2d xy((uv_dist[0] - cam_k[2]) / cam_k[0], (uv_dist[1] - cam_k[3]) / cam_k[1]);

        return xy;
    }

    /**
     * @brief Computes the derivative of raw distorted to normalized coordinate.
     * @param uv_norm Normalized coordinates we wish to distort
     * @param H_dz_dzn Derivative of measurement z in respect to normalized
     * @param H_dz_dzeta Derivative of measurement z in respect to intrinic parameters
     */
    Eigen::Vector2d distortAndCalcJacobian(const Eigen::Vector2d &uv_norm,
                                           Eigen::Matrix2d &H_dz_dzn,
                                           Eigen::MatrixXd &H_dz_dzeta,
                                           bool zeta_has_distortion) const override {
        // Get our camera parameters
        const Eigen::VectorXd &cam_d = m_cameraParam;

        const double fx = cam_d[0];
        const double fy = cam_d[1];
        // const double cx = cam_d[2];
        // const double cy = cam_d[3];

        const double k1 = cam_d[4];
        const double k2 = cam_d[5];
        const double k3 = cam_d[6];
        const double p1 = cam_d[7];
        const double p2 = cam_d[8];

        const double x_u = uv_norm(0);
        const double y_u = uv_norm(1);
        const double x_u2 = x_u * x_u;
        const double y_u2 = y_u * y_u;
        const double xu_yu = x_u * y_u;

        // Calculate distorted coordinates for radial
        const double r2 = x_u2 + y_u2;
        const double r4 = r2 * r2;
        const double r6 = r4 * r2;

        const double radial = 1 + k1 * r2 + k2 * r4 + k3 * r6;

        // Calculate distorted coordinates for radtan
        const double x_d = x_u * radial + p2 * (r2 + 2 * x_u2) + 2 * p1 * xu_yu;
        const double y_d = y_u * radial + p1 * (r2 + 2 * y_u2) + 2 * p2 * xu_yu;

        // Jacobian of distorted pixel to normalized pixel
        const double d_r2_d_xu = 2 * x_u;
        const double d_r4_d_xu = 4 * x_u * r2;
        const double d_d6_d_xu = 6 * x_u * r4;
        const double d_r2_d_yu = 2 * y_u;
        const double d_r4_d_yu = 4 * y_u * r2;
        const double d_r6_d_yu = 6 * y_u * r4;
        const double d_radial_d_xu = k1 * d_r2_d_xu + k2 * d_r4_d_xu + k3 * d_d6_d_xu;
        const double d_radial_d_yu = k1 * d_r2_d_yu + k2 * d_r4_d_yu + k3 * d_r6_d_yu;
        const double d_projx_d_xu = radial + x_u * d_radial_d_xu + p2 * 6.0 * x_u + 2.0 * p1 * y_u;
        const double d_projx_d_yu = y_u * d_radial_d_xu + p1 * 2.0 * x_u + 2.0 * p2 * y_u;
        const double d_projy_d_xu = x_u * d_radial_d_yu + p2 * 2.0 * y_u + 2.0 * p1 * x_u;
        const double d_projy_d_yu = radial + y_u * d_radial_d_yu + p1 * 6.0 * y_u + 2.0 * p2 * x_u;

        H_dz_dzn = Eigen::Matrix2d::Zero();
        H_dz_dzn(0, 0) = fx * d_projx_d_xu;
        H_dz_dzn(0, 1) = fx * d_projx_d_yu;
        H_dz_dzn(1, 0) = fy * d_projy_d_xu;
        H_dz_dzn(1, 1) = fy * d_projy_d_yu;

        if (zeta_has_distortion) {
            // Compute the Jacobian in respect to the intrinsics
            H_dz_dzeta = Eigen::MatrixXd::Zero(2, 9);
            H_dz_dzeta(0, 0) = x_d;
            H_dz_dzeta(0, 2) = 1;
            H_dz_dzeta(0, 4) = fx * x_u * r2;
            H_dz_dzeta(0, 5) = fx * x_u * r4;
            H_dz_dzeta(0, 6) = fx * x_u * r6;
            H_dz_dzeta(0, 7) = fx * 2 * xu_yu;
            H_dz_dzeta(0, 8) = fx * (r2 + 2 * x_u2);
            H_dz_dzeta(1, 1) = y_d;
            H_dz_dzeta(1, 3) = 1;
            H_dz_dzeta(1, 4) = fy * y_u * r2;
            H_dz_dzeta(1, 5) = fy * y_u * r4;
            H_dz_dzeta(1, 6) = fy * y_u * r6;
            H_dz_dzeta(1, 7) = fy * (r2 + 2 * y_u2);
            H_dz_dzeta(1, 8) = fy * 2 * xu_yu;
        } else {
            // Compute the Jacobian in respect to the intrinsics
            H_dz_dzeta = Eigen::MatrixXd::Zero(2, 4);
            H_dz_dzeta(0, 0) = x_d;
            H_dz_dzeta(0, 2) = 1;
            H_dz_dzeta(1, 1) = y_d;
            H_dz_dzeta(1, 3) = 1;
        }

        Eigen::Vector4d cam_k = m_cameraParam.segment<4>(0);

        // Calculate distorted coordinates for fisheye
        Eigen::Vector2d uv_dist;
        uv_dist(0) = cam_k(0) * x_d + cam_k(2);
        uv_dist(1) = cam_k(1) * y_d + cam_k(3);
        return uv_dist;
    }

    // innner use
    template <typename Scalar, typename DerivedJd2uPtr = std::nullptr_t>
    inline Eigen::Vector<Scalar, 2> distortInner(const Eigen::Vector<Scalar, 2> &uv_norm,
                                                 DerivedJd2uPtr Jd2u = nullptr) const {
        constexpr bool NoJacobian = std::is_same_v<DerivedJd2uPtr, std::nullptr_t>;

        const auto distortion = m_cameraParam.segment<5>(4).template cast<Scalar>();

        const Scalar k1 = distortion[0];
        const Scalar k2 = distortion[1];
        const Scalar k3 = distortion[2];
        const Scalar p1 = distortion[3];
        const Scalar p2 = distortion[4];

        const Scalar x_u = uv_norm[0];
        const Scalar y_u = uv_norm[1];
        const Scalar x_u2 = x_u * x_u;
        const Scalar y_u2 = y_u * y_u;
        const Scalar xu_yu = x_u * y_u;
        const Scalar r2 = x_u2 + y_u2;
        const Scalar r4 = r2 * r2;
        const Scalar r6 = r4 * r2;

        const Scalar radial = 1 + k1 * r2 + k2 * r4 + k3 * r6;

        const Scalar x_d = x_u * radial + p2 * (r2 + 2 * x_u2) + 2 * p1 * xu_yu;
        const Scalar y_d = y_u * radial + p1 * (r2 + 2 * y_u2) + 2 * p2 * xu_yu;

        if constexpr (!NoJacobian) {
            const double d_r2_d_xu = 2 * x_u;
            const double d_r4_d_xu = 4 * x_u * r2;
            const double d_d6_d_xu = 6 * x_u * r4;
            const double d_r2_d_yu = 2 * y_u;
            const double d_r4_d_yu = 4 * y_u * r2;
            const double d_r6_d_yu = 6 * y_u * r4;
            const double d_radial_d_xu = k1 * d_r2_d_xu + k2 * d_r4_d_xu + k3 * d_d6_d_xu;
            const double d_radial_d_yu = k1 * d_r2_d_yu + k2 * d_r4_d_yu + k3 * d_r6_d_yu;
            const double d_projx_d_xu = radial + x_u * d_radial_d_xu + p2 * 6.0 * x_u + 2.0 * p1 * y_u;
            const double d_projx_d_yu = y_u * d_radial_d_xu + p1 * 2.0 * x_u + 2.0 * p2 * y_u;
            const double d_projy_d_xu = x_u * d_radial_d_yu + p2 * 2.0 * y_u + 2.0 * p1 * x_u;
            const double d_projy_d_yu = radial + y_u * d_radial_d_yu + p1 * 6.0 * y_u + 2.0 * p2 * x_u;
            (*Jd2u)(0, 0) = d_projx_d_xu;
            (*Jd2u)(0, 1) = d_projx_d_yu;
            (*Jd2u)(1, 0) = d_projy_d_xu;
            (*Jd2u)(1, 1) = d_projy_d_yu;
        }

        return Eigen::Vector<Scalar, 2>(x_d, y_d);
    }

   private:
    template <typename Scalar, typename DerivedJd2uPtr = std::nullptr_t, typename DerivedJu2intrPtr = std::nullptr_t>
    Eigen::Vector<Scalar, 2> distortImpl(const Eigen::Vector<Scalar, 2> &uv_norm,
                                         DerivedJd2uPtr Juv2u = nullptr,
                                         DerivedJu2intrPtr Juv2intr = nullptr) const {
        constexpr bool NoJacobian1 = std::is_same_v<DerivedJd2uPtr, std::nullptr_t>;
        constexpr bool NoJacobian2 = std::is_same_v<DerivedJu2intrPtr, std::nullptr_t>;
        constexpr bool NoJacobian = NoJacobian1 && NoJacobian2;
        if constexpr (!NoJacobian1) {
            EIGEN_STATIC_ASSERT(std::is_pointer_v<DerivedJd2uPtr>, THIS_METHOD_IS_ONLY_FOR_POINTER);
            using DerivedJd2u = typename std::remove_pointer<DerivedJd2uPtr>::type;
            EIGEN_STATIC_ASSERT((std::is_same_v<Scalar, typename DerivedJd2u::Scalar>),
                                THIS_METHOD_IS_ONLY_FOR_MATRICES_OF_SAME_SCALAR_TYPE);
            EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(DerivedJd2u, 2, 2);
        }

        if constexpr (!NoJacobian2) {
            EIGEN_STATIC_ASSERT(std::is_pointer_v<DerivedJu2intrPtr>, THIS_METHOD_IS_ONLY_FOR_POINTER);
            using DerivedJu2intr = typename std::remove_pointer<DerivedJu2intrPtr>::type;
            EIGEN_STATIC_ASSERT((std::is_same_v<Scalar, typename DerivedJu2intr::Scalar>),
                                THIS_METHOD_IS_ONLY_FOR_MATRICES_OF_SAME_SCALAR_TYPE);
            // EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(DerivedJu2intr, 2, 4);
        }

        const auto cam_k = m_cameraParam.segment<4>(0).template cast<Scalar>();

        Eigen::Vector<Scalar, 2> uv_dist_norm;
        if constexpr (NoJacobian) {  // no jacobian
            uv_dist_norm = distortInner(uv_norm);
        } else if constexpr (NoJacobian2) {  // only uv norm jacobian
            uv_dist_norm = distortInner(uv_norm, Juv2u);
            (*Juv2u).row(0) *= cam_k(0);
            (*Juv2u).row(1) *= cam_k(1);
        } else {  // full jacobian
            uv_dist_norm = distortInner(uv_norm, Juv2u);
            (*Juv2u).row(0) *= cam_k(0);
            (*Juv2u).row(1) *= cam_k(1);

            Juv2intr->setZero();
            (*Juv2intr)(0, 0) = uv_dist_norm(0);
            (*Juv2intr)(0, 2) = 1;
            (*Juv2intr)(1, 1) = uv_dist_norm(1);
            (*Juv2intr)(1, 3) = 1;
        }

        // Calculate distorted coordinates for fisheye
        Eigen::Vector<Scalar, 2> uv_dist
            = cam_k.template head<2>().array() * uv_dist_norm.array() + cam_k.template tail<2>().array();
        return uv_dist;
    }

    /**
     * @brief Given a raw uv point, this will undistort it based on the camera matrices into normalized camera coords.
     * @param uv_dist Raw uv coordinate we wish to undistort
     * @return 2d vector of normalized coordinates
     */
    template <typename Scalar>
    inline Eigen::Vector<Scalar, 2> undistortImpl(const Eigen::Vector<Scalar, 2> &uv_dist) const {
        // fx, fy, cx, cy
        const auto cam_k = m_cameraParam.segment<4>(0).template cast<Scalar>();
        Eigen::Vector<Scalar, 2> xy((uv_dist[0] - cam_k[2]) / cam_k[0], (uv_dist[1] - cam_k[3]) / cam_k[1]);

        bool coverage = false;
        Eigen::Vector<Scalar, 2> xy_u = xy;
        Eigen::Matrix<Scalar, 2, 2> Jd2u, Jd2u_inv;
        for (int i = 0; i < MAX_UNDIST_ITER; i++) {
            Eigen::Vector<Scalar, 2> xy_d = distortInner(xy_u, &Jd2u);

            Scalar det = Jd2u(1, 1) * Jd2u(0, 0) - Jd2u(0, 1) * Jd2u(1, 0);
            if (fabs(det) < pico_common::eps<Scalar>()) {
                break;
            }
            Scalar inv_det = 1 / det;
            Jd2u_inv << Jd2u(1, 1), -Jd2u(0, 1), -Jd2u(1, 0), Jd2u(0, 0);
            Jd2u_inv *= inv_det;

            Eigen::Vector<Scalar, 2> e(xy - xy_d);
            Eigen::Vector<Scalar, 2> du = Jd2u_inv * e;
            xy_u += du;

            if (e.dot(e) < MAX_UNDIST_THRE) {
                coverage = true;
                break;
            }
        }

        if (!coverage) {
            // printf("[cam_core]: CamPinhole32 undistort not coverage\n");
        }

        return xy_u;
    }
};

}  // namespace pico_cam
