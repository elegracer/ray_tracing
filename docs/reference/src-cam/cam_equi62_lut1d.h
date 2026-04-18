#pragma once

#include "cam_base.h"

namespace pico_cam {

class CamEqui62Lut1D final : public CamBase {
   public:
    /**
     * @brief Default constructor
     * @param width Width of the camera (raw pixels)
     * @param height Height of the camera (raw pixels)
     */
    CamEqui62Lut1D(int width, int height, cv::Mat mask = cv::Mat()) : CamBase(width, height, mask, CAM_EQUI62_LUT1D) {
        m_cameraModel = "pinhole";
        m_distortionModel = "equiDis62";
    }

    ~CamEqui62Lut1D() {
    }

    /**
     * @brief This will set and update the camera calibration values.
     * This should be called on startup for each camera and after update!
     * @param param Camera calibration information
     */
    void setParam(const Eigen::VectorXd &param) override {
        // equi_camera is (fx & fy & cx & cy & k1 & k2 & k3 & k4 & k5 & k6 & p1 & p2)
        assert(param.rows() == 12);
        m_cameraParam = param;

        if (!m_tab_inited) {
            double fx = param[0];
            double fy = param[1];
            double cx = param[2];
            double cy = param[3];

            double x = ((m_width + 10) - cx) / fx;
            double y = ((m_height + 10) - cy) / fy;

            m_steps = std::sqrt(x * x + y * y) / LUT_SIZE_1D;
            for (int i = 0; i < LUT_SIZE_1D; i++) {
                m_lut[i] = undistortPolynomialRadial(m_steps * i);
            }
            printf("ERROR arm init lut, this cannot usually be called\n");

            m_tab_inited = true;
        }
    }
    void setProjParam(const Eigen::VectorXd &proj_param) override {
        assert(m_tab_inited && proj_param.rows() == 4);
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
        // first step, rad distortion
        double xr, yr;
        {
            double x = uv_norm[0];
            double y = uv_norm[1];

            double cdist;
            {
                double r2 = x * x + y * y;
                double r = std::sqrt(r2);

                // angle of the incoming ray:
                double theta = std::atan(r);
                cdist = r > 1e-8 ? theta / r : 1;

                // jacobian
                if (r > 1e-8) {
                    double atr2r = 1 / (1 + r2);
                    double Jtheta2x = atr2r * x / r;
                    double Jtheta2y = atr2r * y / r;

                    double Jcdist2x = (Jtheta2x * r - theta * x / r) / r2;
                    double Jcdist2y = (Jtheta2y * r - theta * y / r) / r2;

                    Jd2u(0, 0) = cdist + x * Jcdist2x;
                    Jd2u(0, 1) = x * Jcdist2y;
                    Jd2u(1, 0) = Jd2u(0, 1);
                    Jd2u(1, 1) = cdist + y * Jcdist2y;
                } else {
                    Jd2u(0, 0) = 1;
                    Jd2u(0, 1) = 0;
                    Jd2u(1, 0) = 0;
                    Jd2u(1, 1) = 1;
                }
            }

            xr = x * cdist;
            yr = y * cdist;
        }

        Eigen::Vector4d cam_k = m_cameraParam.segment<4>(0);

        // Calculate distorted coordinates for fisheye
        Eigen::Vector2d uv_dist;
        uv_dist(0) = cam_k(0) * xr + cam_k(2);
        uv_dist(1) = cam_k(1) * yr + cam_k(3);
        Jd2u.row(0) *= cam_k(0);
        Jd2u.row(1) *= cam_k(1);

        return uv_dist;
    }

    Eigen::Vector2d undistortNoDistortParam(const Eigen::Vector2d &uv_dist) const override {
        // fx, fy, cx, cy
        Eigen::Vector4d cam_k = m_cameraParam.segment<4>(0);
        Eigen::Vector2d xy((uv_dist[0] - cam_k[2]) / cam_k[0], (uv_dist[1] - cam_k[3]) / cam_k[1]);

        double theta = xy.norm();
        if (theta < pico_common::D_EPS) {
            return xy;
        }

        double tmp = tan(theta) / theta;
        return Eigen::Vector2d(tmp * xy[0], tmp * xy[1]);
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
        double fx = m_cameraParam[0];
        double fy = m_cameraParam[1];
        double cx = m_cameraParam[2];
        double cy = m_cameraParam[3];

        Eigen::Vector6d m_rad = m_cameraParam.segment<6>(4);
        Eigen::Vector2d m_tan = m_cameraParam.segment<2>(10);

        // first step, rad distortion
        double x = uv_norm[0];
        double y = uv_norm[1];

        double r2 = x * x + y * y;
        double r = std::sqrt(r2);

        // angle of the incoming ray:
        double theta = std::atan(r);
        double theta2 = theta * theta;
        double theta4 = theta2 * theta2;
        double theta6 = theta4 * theta2;
        double theta8 = theta6 * theta2;
        double theta10 = theta8 * theta2;
        double theta12 = theta10 * theta2;

        double thetad = theta
                        * (1 + m_rad[0] * theta2 + m_rad[1] * theta4 + m_rad[2] * theta6 + m_rad[3] * theta8
                           + m_rad[4] * theta10 + m_rad[5] * theta12);

        double cdist = r > 1e-8 ? thetad / r : 1;
        double xr = x * cdist;
        double yr = y * cdist;

        // second step, tan distortion
        double xr2 = xr * xr;
        double yr2 = yr * yr;
        double xryr = xr * yr;
        double x_distort = xr + 2 * m_tan[0] * xryr + m_tan[1] * (xr2 + yr2 + 2 * xr2);
        double y_distort = yr + m_tan[0] * (xr2 + yr2 + 2 * yr2) + 2 * m_tan[1] * xryr;

        // project to uv
        double u = fx * x_distort + cx;
        double v = fy * y_distort + cy;
        (void)u;
        (void)v;

        //////////////////////////////////////////////////////////////////////////////////////////
        Eigen::Matrix<double, 2, 4> Juv2intrin;
        {
            Juv2intrin(0, 0) = x_distort;
            Juv2intrin(0, 1) = 0;
            Juv2intrin(0, 2) = 1;
            Juv2intrin(0, 3) = 0;

            Juv2intrin(1, 0) = 0;
            Juv2intrin(1, 1) = y_distort;
            Juv2intrin(1, 2) = 0;
            Juv2intrin(1, 3) = 1;
        }
        Eigen::Matrix2d Juv2d;
        {
            Juv2d(0, 0) = fx;
            Juv2d(0, 1) = 0;

            Juv2d(1, 0) = 0;
            Juv2d(1, 1) = fy;
        }
        Eigen::Matrix2d Jd2xryr;
        {
            Jd2xryr(0, 0) = 1 + 2 * m_tan[0] * yr + m_tan[1] * (2 * xr + 4 * xr);
            Jd2xryr(0, 1) = 2 * m_tan[0] * xr + m_tan[1] * (2 * yr);
            Jd2xryr(1, 0) = Jd2xryr(0, 1);
            Jd2xryr(1, 1) = 1 + 2 * m_tan[1] * xr + m_tan[0] * (2 * yr + 4 * yr);
        }
        Eigen::Matrix2d Jxryr2un = Eigen::Matrix2d::Identity();
        if (r > 1e-8) {
            double Jthetad2theta = 1 + 3 * m_rad[0] * theta2 + 5 * m_rad[1] * theta4 + 7 * m_rad[2] * theta6
                                   + 9 * m_rad[3] * theta8 + 11 * m_rad[4] * theta10 + 13 * m_rad[5] * theta12;

            double atr2r = 1 / (1 + r2);
            double Jtheta2x = atr2r * x / r;
            double Jtheta2y = atr2r * y / r;

            double Jthetad2x = Jthetad2theta * Jtheta2x;
            double Jthetad2y = Jthetad2theta * Jtheta2y;

            double Jcdist2x = (Jthetad2x * r - thetad * x / r) / r2;
            double Jcdist2y = (Jthetad2y * r - thetad * y / r) / r2;

            Jxryr2un(0, 0) = cdist + x * Jcdist2x;
            Jxryr2un(0, 1) = x * Jcdist2y;
            Jxryr2un(1, 0) = Jxryr2un(0, 1);
            Jxryr2un(1, 1) = cdist + y * Jcdist2y;
        }
        H_dz_dzn = Juv2d * Jd2xryr * Jxryr2un;

        if (zeta_has_distortion) {
            Eigen::Matrix<double, 2, 6> Jxryr2radparam = Eigen::Matrix<double, 2, 6>::Zero();
            if (r > 1e-8) {
                double inv_r = 1 / r;
                Jxryr2radparam(0, 0) = x * inv_r * theta * theta2;
                Jxryr2radparam(0, 1) = Jxryr2radparam(0, 0) * theta2;
                Jxryr2radparam(0, 2) = Jxryr2radparam(0, 1) * theta2;
                Jxryr2radparam(0, 3) = Jxryr2radparam(0, 2) * theta2;
                Jxryr2radparam(0, 4) = Jxryr2radparam(0, 3) * theta2;
                Jxryr2radparam(0, 5) = Jxryr2radparam(0, 4) * theta2;

                Jxryr2radparam(1, 0) = y * inv_r * theta * theta2;
                Jxryr2radparam(1, 1) = Jxryr2radparam(1, 0) * theta2;
                Jxryr2radparam(1, 2) = Jxryr2radparam(1, 1) * theta2;
                Jxryr2radparam(1, 3) = Jxryr2radparam(1, 2) * theta2;
                Jxryr2radparam(1, 4) = Jxryr2radparam(1, 3) * theta2;
                Jxryr2radparam(1, 5) = Jxryr2radparam(1, 4) * theta2;
            }
            Eigen::Matrix2d Jd2tanparam;
            {
                Jd2tanparam(0, 0) = 2 * xryr;
                Jd2tanparam(0, 1) = xr2 + yr2 + 2 * xr2;
                Jd2tanparam(1, 0) = xr2 + yr2 + 2 * yr2;
                Jd2tanparam(1, 1) = 2 * xryr;
            }

            H_dz_dzeta = Eigen::MatrixXd::Zero(2, 12);
            H_dz_dzeta << Juv2intrin, Juv2d * Jd2xryr * Jxryr2radparam, Juv2d * Jd2tanparam;
        } else {
            H_dz_dzeta = Eigen::MatrixXd::Zero(2, 4);
            H_dz_dzeta << Juv2intrin;
        }

        return Eigen::Vector2d(u, v);
    }

    // innner use
    template <typename Scalar, typename DerivedJd2uPtr = std::nullptr_t>
    inline Eigen::Vector<Scalar, 2> distortInner(const Eigen::Vector<Scalar, 2> &uv_norm,
                                                 DerivedJd2uPtr Jd2u = nullptr) const {
        constexpr bool NoJacobian = std::is_same_v<DerivedJd2uPtr, std::nullptr_t>;

        const auto m_rad = m_cameraParam.segment<6>(4).template cast<Scalar>();
        const auto m_tan = m_cameraParam.segment<2>(10).template cast<Scalar>();

        Scalar x_distort, y_distort;
        {
            // first step, rad distortion
            const Scalar x = uv_norm[0];
            const Scalar y = uv_norm[1];

            const Scalar r2 = uv_norm.squaredNorm();
            const Scalar r = std::sqrt(r2);

            // angle of the incoming ray:
            const Scalar theta = std::atan(r);
            const Scalar theta2 = theta * theta;
            const Scalar theta4 = theta2 * theta2;
            const Scalar theta6 = theta4 * theta2;
            const Scalar theta8 = theta6 * theta2;
            const Scalar theta10 = theta8 * theta2;
            const Scalar theta12 = theta10 * theta2;

            const Scalar thetad = theta
                                  * (1 + m_rad[0] * theta2 + m_rad[1] * theta4 + m_rad[2] * theta6 + m_rad[3] * theta8
                                     + m_rad[4] * theta10 + m_rad[5] * theta12);

            const Scalar cdist = r > 1e-8 ? thetad / r : 1;
            const Scalar xr = x * cdist;
            const Scalar yr = y * cdist;

            // second step, tan distortion
            {
                const Scalar xr2 = xr * xr;
                const Scalar yr2 = yr * yr;
                const Scalar xryr = xr * yr;

                x_distort = xr + 2 * m_tan[0] * xryr + m_tan[1] * (xr2 + yr2 + 2 * xr2);
                y_distort = yr + 2 * m_tan[1] * xryr + m_tan[0] * (xr2 + yr2 + 2 * yr2);
            }

            // jacobian
            if constexpr (!NoJacobian) {
                // first step, rad distortion
                Eigen::Matrix<Scalar, 2, 2> Jrad;
                if (r > 1e-8) {
                    const Scalar Jthetad2theta = 1 + 3 * m_rad[0] * theta2 + 5 * m_rad[1] * theta4
                                                 + 7 * m_rad[2] * theta6 + 9 * m_rad[3] * theta8
                                                 + 11 * m_rad[4] * theta10 + 13 * m_rad[5] * theta12;

                    const Scalar atr2r = 1 / (1 + r2);
                    const Scalar Jtheta2x = atr2r * x / r;
                    const Scalar Jtheta2y = atr2r * y / r;

                    const Scalar Jthetad2x = Jthetad2theta * Jtheta2x;
                    const Scalar Jthetad2y = Jthetad2theta * Jtheta2y;

                    const Scalar Jcdist2x = (Jthetad2x * r - thetad * x / r) / r2;
                    const Scalar Jcdist2y = (Jthetad2y * r - thetad * y / r) / r2;

                    Jrad(0, 0) = cdist + x * Jcdist2x;
                    Jrad(0, 1) = x * Jcdist2y;
                    Jrad(1, 0) = Jrad(0, 1);
                    Jrad(1, 1) = cdist + y * Jcdist2y;
                } else {
                    Jrad.setIdentity();
                }

                // second step, tan distortion
                Eigen::Matrix<Scalar, 2, 2> Jtan;
                {
                    Jtan(0, 0) = 1 + 2 * m_tan[0] * yr + m_tan[1] * (2 * xr + 4 * xr);
                    Jtan(0, 1) = 2 * m_tan[0] * xr + m_tan[1] * (2 * yr);
                    Jtan(1, 0) = Jtan(0, 1);
                    Jtan(1, 1) = 1 + 2 * m_tan[1] * xr + m_tan[0] * (2 * yr + 4 * yr);
                }

                // third step, combine
                Jd2u->noalias() = Jtan * Jrad;
            }
        }

        return Eigen::Vector<Scalar, 2>(x_distort, y_distort);
    }

    // inner use
    double undistortPolynomialRadial(double rd) {
        // rd = r + k1r3 + k2r5 + k3r7 + k4r9 + k5r11 + k6r13;
        Eigen::Vector6d k = m_cameraParam.segment<6>(4);

        // this initial value is better than rd ?
        double r = pow(rd, 0.3333333333);
        for (int i = 0; i < 30; i++) {
            double r2 = r * r;
            double r4 = r2 * r2;
            double r6 = r4 * r2;
            double r8 = r6 * r2;
            double r10 = r8 * r2;
            double r12 = r10 * r2;

            double rd_est = r * (1 + k[0] * r2 + k[1] * r4 + k[2] * r6 + k[3] * r8 + k[4] * r10 + k[5] * r12);
            double error = rd - rd_est;
            if (fabs(error) < pico_common::D_EPS) {
                break;
            }

            double J
                = 1 + 3 * k[0] * r2 + 5 * k[1] * r4 + 7 * k[2] * r6 + 9 * k[3] * r8 + 11 * k[4] * r10 + 13 * k[5] * r12;
            double delta_r = error / J;

            if (fabs(delta_r) < pico_common::D_EPS) {
                break;
            }

            if (fabs(delta_r) > 3.1416) {
                break;
            }

            r += delta_r;
        }
        return r;
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
        const auto m_tan = m_cameraParam.segment<2>(10).template cast<Scalar>();
        Eigen::Vector<Scalar, 2> xy((uv_dist[0] - cam_k[2]) / cam_k[0], (uv_dist[1] - cam_k[3]) / cam_k[1]);

        if (xy.norm() < pico_common::eps<Scalar>()) {
            return xy;
        }

        Eigen::Vector<Scalar, 2> xy_u = xy;

        if (!m_tab_inited) {
            bool coverage = false;
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
                // printf("[cam_core]: CamEqui62Lut1D undistort not coverage\n");
            }
        } else {
            // remove tan distortion
            Scalar xr, yr;
            {
                xr = xy[0];
                yr = xy[1];

                Scalar xr2 = xr * xr;
                Scalar yr2 = yr * yr;
                Scalar xryr = xr * yr;

                Scalar dx = xr + 2 * m_tan[0] * xryr + m_tan[1] * (xr2 + yr2 + 2 * xr2);
                Scalar dy = yr + 2 * m_tan[1] * xryr + m_tan[0] * (xr2 + yr2 + 2 * yr2);

                Eigen::Matrix<Scalar, 2, 2> Jtan;
                Jtan(0, 0) = 1 + 2 * m_tan[0] * yr + m_tan[1] * (2 * xr + 4 * xr);
                Jtan(0, 1) = 2 * m_tan[0] * xr + m_tan[1] * (2 * yr);
                Jtan(1, 0) = Jtan(0, 1);
                Jtan(1, 1) = 1 + 2 * m_tan[1] * xr + m_tan[0] * (2 * yr + 4 * yr);

                Scalar det = Jtan(1, 1) * Jtan(0, 0) - Jtan(0, 1) * Jtan(1, 0);
                if (fabs(det) > pico_common::eps<Scalar>()) {
                    Eigen::Matrix<Scalar, 2, 2> Jtan_inv;

                    Scalar inv_det = 1 / det;
                    Jtan_inv << Jtan(1, 1), -Jtan(0, 1), -Jtan(1, 0), Jtan(0, 0);
                    Jtan_inv *= inv_det;

                    Eigen::Vector<Scalar, 2> error(xy[0] - dx, xy[1] - dy);
                    Eigen::Vector<Scalar, 2> delta = Jtan_inv * error;

                    xr += delta[0];
                    yr += delta[1];

                } else {
                    printf("[cam_core]: CamEqui62Lut1D remove tan distortion singularity\n");
                }
            }

            // use look up table to remove rad distortion
            {
                Scalar rd = std::sqrt(xr * xr + yr * yr);
                int r_s = (int)(rd / (Scalar)m_steps);
                if ((r_s < 0) || (r_s >= (LUT_SIZE_1D - 1))) {
                    printf("[cam_core]: CamEqui62Lut1D look up table out of bound ERROR, %d  %lf/%lf\n",
                           r_s,
                           uv_dist[0],
                           uv_dist[1]);
                    return xy_u;
                }

                Scalar theta = (rd - m_steps * r_s) * m_lut[r_s + 1] + (m_steps * (1 + r_s) - rd) * m_lut[r_s];
                theta /= m_steps;

                Scalar tmp = std::tan(theta) / rd;
                xy_u[0] = xr * tmp;
                xy_u[1] = yr * tmp;
            }
        }

        return xy_u;
    }

   protected:
    // look up table things
    double m_steps;
    double m_lut[LUT_SIZE_1D];
    bool m_tab_inited = false;
};

}  // namespace pico_cam
