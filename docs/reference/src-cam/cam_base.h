#pragma once

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <Eigen/Core>
#include <Eigen/Dense>
#include <memory>
#include <opencv2/core/eigen.hpp>

// CAUTION: conflict
// clang-format off
#include "common/matrix_utils.h"
#include <opencv2/opencv.hpp>
// clang-format on

#include "cam_type.h"
#include "common/android_log.h"

namespace pico_cam {

class CamBase {
   public:
    CamBase(int width, int height, cv::Mat mask, CamType cam_type)
        : m_width(width), m_height(height), m_cameraType(cam_type), m_mask(mask) {
        if (width > HIGH_RESOLUTION_THRE || height > HIGH_RESOLUTION_THRE) {
            m_highResolutionFlag = true;
        } else {
            m_highResolutionFlag = false;
        }
    }

    virtual ~CamBase() {
    }

    /**
     * @brief This will set and update the camera calibration values.
     * This should be called on startup for each camera and after update!
     * @param calib Camera calibration information
     */
    virtual void setParam(const Eigen::VectorXd &calib) = 0;
    virtual void setProjParam(const Eigen::VectorXd &calib) = 0;
    virtual Eigen::Vector2d getFxfy() const = 0;
    virtual Eigen::Vector2d getCxcy() const = 0;
    cv::Mat getMask() const {
        return m_mask;
    }

    /**
     * @brief This function will check if the given pixel lies in the camera frame with required margin.
     * @param px 2d pixel coordinate (level 0)
     * @param margin (optional) required margin size, 0 when default
     * @return true if the pixel lies in the frame with the given margin, otherwise
     * @return false
     */
    bool isInFrame(const Eigen::Vector2i &px, int margin = 0) const {
        if (px[0] >= margin && px[1] >= margin && px[0] < m_width - margin && px[1] < m_height - margin
            && (*m_mask.ptr<uint8_t>(px[1], px[0]) < 127)) {
            return true;
        }
        return false;
    }

    /**
     * @brief Given a 3d point in camera frame, project it into normalized uv coordinate
     * @param xyz 3d point in camera frame
     * @param uv_norm we get in Normalized coordinates
     * @return if project success, true is sucess, false is failed
     */
    template <typename Derived>
    Eigen::Vector<typename Derived::Scalar, 2> project(const Eigen::MatrixBase<Derived> &xyz) const {
        checkScalarType<typename Derived::Scalar>();
        EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived, 3);
        using EvalOrRef = typename Eigen::internal::eval<Derived>::type;
        const EvalOrRef xyz_eval(xyz);
        if (xyz_eval[2] <= 0) {
            return Eigen::Vector<typename Derived::Scalar, 2>(-1000, -1000);
        }

        Eigen::Vector<typename Derived::Scalar, 2> uv_norm = xyz_eval.template head<2>() / xyz_eval[2];
        if (uv_norm.squaredNorm() > MAX_HALF_FOV_RAD_SQ) {
            return Eigen::Vector<typename Derived::Scalar, 2>(-1000, -1000);
        }

        return uv_norm;
    }

    template <typename Derived>
    Eigen::Vector<typename Derived::Scalar, 2> project(const Eigen::MatrixBase<Derived> &xyz,
                                                       Eigen::Matrix<typename Derived::Scalar, 2, 3> &dzn_dxyz) const {
        checkScalarType<typename Derived::Scalar>();
        EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived, 3);
        using EvalOrRef = typename Eigen::internal::eval<Derived>::type;
        const EvalOrRef xyz_eval(xyz);
        if (xyz_eval[2] <= 0) {
            dzn_dxyz.setZero();
            return Eigen::Vector<typename Derived::Scalar, 2>(-1000, -1000);
        }

        const typename Derived::Scalar invZ = typename Derived::Scalar(1) / xyz_eval[2];
        Eigen::Vector<typename Derived::Scalar, 2> uv_norm = xyz_eval.template head<2>() * invZ;
        if (uv_norm.squaredNorm() > MAX_HALF_FOV_RAD_SQ) {
            dzn_dxyz.setZero();
            return Eigen::Vector<typename Derived::Scalar, 2>(-1000, -1000);
        }
        dzn_dxyz << invZ, 0, -uv_norm[0] * invZ, 0, invZ, -uv_norm[1] * invZ;
        return uv_norm;
    }

    template <typename Derived>
    Eigen::Vector<typename Derived::Scalar, 2> world2pixel(const Eigen::MatrixBase<Derived> &xyz) const {
        checkScalarType<typename Derived::Scalar>();
        EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived, 3);
        using EvalOrRef = typename Eigen::internal::eval<Derived>::type;
        const EvalOrRef xyz_eval(xyz);
        Eigen::Vector<typename Derived::Scalar, 2> uv_norm = project(xyz_eval);
        if (uv_norm[0] < -999 || uv_norm[1] < -999) {
            return Eigen::Vector<typename Derived::Scalar, 2>(-1000, -1000);
        }
        Eigen::Vector<typename Derived::Scalar, 2> uv_dist = distort(uv_norm);
        return uv_dist;
    }

    template <typename Derived>
    Eigen::Vector<typename Derived::Scalar, 2> world2pixel(
        const Eigen::MatrixBase<Derived> &xyz, Eigen::Matrix<typename Derived::Scalar, 2, 3> &dz_dxyz) const {
        checkScalarType<typename Derived::Scalar>();
        EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived, 3);
        using EvalOrRef = typename Eigen::internal::eval<Derived>::type;
        const EvalOrRef xyz_eval(xyz);
        Eigen::Matrix<typename Derived::Scalar, 2, 3> dzn_dxyz;
        Eigen::Vector<typename Derived::Scalar, 2> uv_norm = project(xyz_eval, dzn_dxyz);
        if (uv_norm[0] < -999 || uv_norm[1] < -999) {
            dz_dxyz.setZero();
            return Eigen::Vector<typename Derived::Scalar, 2>(-1000, -1000);
        }
        Eigen::Matrix<typename Derived::Scalar, 2, 2> dz_dzn;
        Eigen::Vector<typename Derived::Scalar, 2> uv_dist = distort(uv_norm, dz_dzn);
        dz_dxyz.noalias() = dz_dzn * dzn_dxyz;
        return uv_dist;
    }

    template <typename Derived>
    Eigen::Vector<typename Derived::Scalar, 2> world2pixel(
        const Eigen::MatrixBase<Derived> &xyz,
        Eigen::Matrix<typename Derived::Scalar, 2, 3> &dz_dxyz,
        Eigen::Matrix<typename Derived::Scalar, Eigen::Dynamic, Eigen::Dynamic> &dz_dintr) const {
        checkScalarType<typename Derived::Scalar>();
        EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived, 3);
        using EvalOrRef = typename Eigen::internal::eval<Derived>::type;
        const EvalOrRef xyz_eval(xyz);
        Eigen::Matrix<typename Derived::Scalar, 2, 3> dzn_dxyz;
        Eigen::Vector<typename Derived::Scalar, 2> uv_norm = project(xyz_eval, dzn_dxyz);
        if (uv_norm[0] < -999 || uv_norm[1] < -999) {
            dz_dxyz.setZero();
            return Eigen::Vector<typename Derived::Scalar, 2>(-1000, -1000);
        }

        Eigen::Matrix<typename Derived::Scalar, 2, 2> dz_dzn;
        Eigen::Vector<typename Derived::Scalar, 2> uv_dist = distort(uv_norm, dz_dzn, dz_dintr);
        dz_dxyz.noalias() = dz_dzn * dzn_dxyz;
        return uv_dist;
    }

    /**
     * @brief Given a 2d point in normalized uv coordinate, project it into 3d camera frame
     * @param uv_norm in Normalized coordinates
     * @param xyz 3d point we get in camera frame, (norm is 1/z is 1)
     * @return true is project sucess, false is project failed
     */
    template <typename Derived>
    Eigen::Vector<typename Derived::Scalar, 3> unproject(const Eigen::MatrixBase<Derived> &uv_norm) const {
        checkScalarType<typename Derived::Scalar>();
        EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived, 2);
        using EvalOrRef = typename Eigen::internal::eval<Derived>::type;
        const EvalOrRef uv_norm_eval(uv_norm);
        Eigen::Vector<typename Derived::Scalar, 3> xyz{uv_norm_eval[0], uv_norm_eval[1], 1.0};
        return xyz.normalized();
    }

    template <typename Derived>
    Eigen::Vector<typename Derived::Scalar, 3> pixel2world(const Eigen::MatrixBase<Derived> &uv_dist) const {
        checkScalarType<typename Derived::Scalar>();
        EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived, 2);
        using EvalOrRef = typename Eigen::internal::eval<Derived>::type;
        const EvalOrRef uv_dist_eval(uv_dist);
        Eigen::Vector<typename Derived::Scalar, 2> uv_norm = undistort(uv_dist_eval);
        Eigen::Vector<typename Derived::Scalar, 3> xyz = unproject(uv_norm);
        return xyz;
    }

    template <typename Scalar>
    Eigen::Vector<Scalar, 2> calcJacPw2Puv(const Sophus::SE3<Scalar> &Tcw,
                                           const Eigen::Vector<Scalar, 3> &Pw,
                                           Eigen::Matrix<Scalar, 2, 6> &JPw2uv) const {
        checkScalarType<Scalar>();
        Eigen::Matrix<Scalar, 2, 3> d_proj_pc;
        const Eigen::Vector<Scalar, 2> uv = world2pixel(Tcw * Pw, d_proj_pc);
        Eigen::Matrix<Scalar, 3, 6> d_pc_T;
        d_pc_T.template block<3, 3>(0, 0).setIdentity();
        d_pc_T.template block<3, 3>(0, 3) = Sophus::SO3<Scalar>::hat(-Pw);
        JPw2uv.noalias() = -d_proj_pc * Tcw.rotationMatrix() * d_pc_T;

        return uv;
    }

    /**
     * @brief Given a normalized uv coordinate this will distort it to the raw image plane
     * @param uv_norm Normalized coordinates we wish to distort
     * @return 2d vector of raw uv coordinate
     */
    virtual Eigen::Vector2d distort(const Eigen::Vector2d &uv_norm) const = 0;
    virtual Eigen::Vector2f distort(const Eigen::Vector2f &uv_norm) const = 0;
    virtual Eigen::Vector2d distort(const Eigen::Vector2d &uv_norm, Eigen::Matrix2d &Jd2u) const = 0;
    virtual Eigen::Vector2f distort(const Eigen::Vector2f &uv_norm, Eigen::Matrix2f &Jd2u) const = 0;
    virtual Eigen::Vector2d distort(const Eigen::Vector2d &uv_norm,
                                    Eigen::Matrix2d &Jd2u,
                                    Eigen::Ref<Eigen::MatrixXd> Juv2intr) const
        = 0;
    virtual Eigen::Vector2f distort(const Eigen::Vector2f &uv_norm,
                                    Eigen::Matrix2f &Jd2u,
                                    Eigen::Ref<Eigen::MatrixXf> Juv2intr) const
        = 0;
    /**
     * @brief Given a raw uv point, this will undistort it based on the camera matrices into normalized camera coords.
     * @param uv_dist Raw uv coordinate we wish to undistort
     * @return 2d vector of normalized coordinates
     */
    virtual Eigen::Vector2d undistort(const Eigen::Vector2d &uv_dist) const = 0;
    virtual Eigen::Vector2f undistort(const Eigen::Vector2f &uv_dist) const = 0;

    /**
     * same as above, but without distortion parameter, just for epipolar search
     */
    virtual Eigen::Vector2d distortNoDistortParam(const Eigen::Vector2d &uv_norm, Eigen::Matrix2d &Jd2u) const = 0;
    virtual Eigen::Vector2d undistortNoDistortParam(const Eigen::Vector2d &uv_dist) const = 0;

    /**
     * @brief Computes the derivative of raw distorted to normalized coordinate.
     * @param uv_norm Normalized coordinates we wish to distort
     * @param H_dz_dzn Derivative of measurement z in respect to normalized
     * @param H_dz_dzeta Derivative of measurement z in respect to intrinic parameters
     */
    virtual Eigen::Vector2d distortAndCalcJacobian(const Eigen::Vector2d &uv_norm,
                                                   Eigen::Matrix2d &H_dz_dzn,
                                                   Eigen::MatrixXd &H_dz_dzeta,
                                                   bool zeta_has_distortion) const
        = 0;

    // distort temp fit
    cv::Point2f distort_cv(const cv::Point2f &uv_norm) const {
        Eigen::Vector2f ept1{uv_norm.x, uv_norm.y};
        Eigen::Vector2f ept2 = distort(ept1);
        return cv::Point2f{ept2(0), ept2(1)};
    }

    // undistort temp fit
    cv::Point2f undistort_cv(const cv::Point2f &uv_dist) {
        Eigen::Vector2f ept1{uv_dist.x, uv_dist.y};
        Eigen::Vector2f ept2 = undistort(ept1);
        return cv::Point2f{ept2(0), ept2(1)};
    }

    //////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////

    Eigen::VectorXd getParam() const {
        return m_cameraParam;
    }

    // Gets the width of the camera images
    int getWidth() const {
        return m_width;
    }

    // Gets the height of the camera images
    int getHeight() const {
        return m_height;
    }

    // Gets camera type
    CamType getCamType() const {
        return m_cameraType;
    }

    // Gets high resolution camera
    bool getHighResolutionFlag() const {
        return m_highResolutionFlag;
    }

    // Gets camera type
    std::string getCamTypeString() const {
        switch (m_cameraType) {
            case CAM_MEI22:
                return "CAM_MEI22";
            case CAM_PINHOLE22:
                return "CAM_PINHOLE22";
            case CAM_EQUI40:
                return "CAM_EQUI40";
            case CAM_EQUI40_LUT1D:
                return "CAM_EQUI40_LUT1D";
            case CAM_EQUI62:
                return "CAM_EQUI62";
            case CAM_EQUI62_LUT1D:
                return "CAM_EQUI62_LUT1D";
            case CAM_EQUI62_BILINEAR:
                return "CAM_EQUI62_BILINEAR";
            default:
                return "UNKNOWN";
        }
    }

    bool writeCali(const std::string &cali_path,
                   const Eigen::Matrix4d &Tic,
                   double temperature = -101.0,
                   const std::vector<std::vector<double>> &calibInfo = {},
                   bool bBinning = false) {
        // create path
        if (0 != createDirectory(cali_path)) {
            return false;
        }

        cv::FileStorage fs;
        try {
            fs.open(cali_path, cv::FileStorage::WRITE);
        } catch (cv::Exception &err) {
            PRINT_INFO("[writeCali]: writeCali cannot open cali file ERROR: %s\n", cali_path.c_str());
            return false;
        }
        PRINT_INFO("[writeCali]: writeCali success %s\n", cali_path.c_str());

        // sensor_type
        {
            std::string sensor_type = "camera";
            std::string comment = "VI-Sensor";
            fs << "sensor_type" << sensor_type;
            fs << "comment" << comment;
            fs.writeComment("\n");
        }

        // Tic
        {
            std::vector<double> cv_Tic(16);
            Eigen::Map<Eigen::Matrix<double, 4, 4, Eigen::RowMajor>> cv_Tic_map(cv_Tic.data());
            cv_Tic_map = Tic;
            fs << "T_BS" << "{" << "rows" << 4 << "cols" << 4 << "data" << cv_Tic  //
               << "}";
            fs.writeComment("\n");
        }

        // resolution
        {
            int rate_hz = 30;
            std::vector<int> resolution{m_width, m_height};
            if (bBinning) {
                resolution[0] *= 2;
                resolution[1] *= 2;
            }
            fs << "rate_hz" << rate_hz;
            fs << "resolution" << resolution;
        }

        // intrinsic and distortion
        {
            int intrinsic_size = 4;
            if (m_cameraType == CAM_MEI22) {
                intrinsic_size = 5;
            }
            int distortion_size = m_cameraParam.rows() - intrinsic_size;
            std::vector<double> cv_intrinsic(intrinsic_size);
            std::vector<double> cv_distortion(distortion_size);
            Eigen::Map<Eigen::VectorXd>(cv_distortion.data(), distortion_size) = m_cameraParam.tail(distortion_size);
            Eigen::Map<Eigen::VectorXd> camIntr(cv_intrinsic.data(), intrinsic_size);
            camIntr = m_cameraParam.head(intrinsic_size);
            if (bBinning) {
                camIntr.tail(4) *= 2;
            }
            fs << "camera_model" << m_cameraModel;
            fs << "intrinsics" << cv_intrinsic;
            fs << "distortion_model" << m_distortionModel;
            fs << "distortion_coefficients" << cv_distortion;
            fs.writeComment("\n");
        }

        // write time
        {
            time_t utc_sec = time(nullptr);
            char curr_time[128];
            {
                struct timeval tv;
                struct timezone tz;
                gettimeofday(&tv, &tz);
                struct tm *t = localtime(&tv.tv_sec);
                sprintf(curr_time,
                        "%d-%d-%d %d:%d:%d",
                        1900 + t->tm_year,
                        1 + t->tm_mon,
                        t->tm_mday,
                        t->tm_hour,
                        t->tm_min,
                        t->tm_sec);
            }
            fs << "calibration_time" << (int)utc_sec;
            fs << "calibration_date" << std::string(curr_time);
        }

        for (size_t i = 0; i < calibInfo.size(); i++) {
            std::string lt_name = "lt" + std::to_string(i);
            fs << lt_name << calibInfo[i];
        }

        // Write temperature
        { fs << "temperature" << temperature; }

        fs.release();
        return true;
    }

    // fov thre
    static double MAX_HALF_FOV_RAD;
    static double MAX_HALF_FOV_RAD_SQ;

   private:
    template <typename Scalar>
    constexpr inline void checkScalarType() const {
        EIGEN_STATIC_ASSERT(std::is_floating_point_v<Scalar>, THIS_METHOD_IS_ONLY_FOR_DOUBLE_OF_FLOAT);
    }

   protected:
    // Cannot construct the base camera class, needs a distortion model
    CamBase() = default;

    // pinhole22 is (fx & fy & cx & cy & k1 & k2 & p1 & p2)
    // mei22     is (xi & fx & fy & cx & cy & k1 & k2 & p1 & p2)
    // equi40    is (fx & fy & cx & cy & k1 & k2 & k3 & k4)
    // equi62    is (fx & fy & cx & cy & k1 & k2 & k3 & k4 & k5 & k6 & p1 & p2)
    Eigen::VectorXd m_cameraParam;

    // Width/Height of the camera (raw pixels)
    int m_width, m_height;
    bool m_highResolutionFlag;
    CamType m_cameraType;
    cv::Mat m_mask;

    // for save online cali
    std::string m_cameraModel;
    std::string m_distortionModel;
    int createDirectory(std::string path, mode_t mode = 0775) {
        if (path.empty()) {
            return -1;
        }

        int len = path.length();
        if (path[len - 1] != '/' && path.find('.') == std::string::npos) {
            path += '/';
        }

        char tmp_path[256] = {0};
        for (int i = 0; i < len; i++) {
            tmp_path[i] = path[i];
            if (tmp_path[i] == '/') {
                if (access(tmp_path, 0) == -1) {
                    if (mkdir(tmp_path, mode) == -1) {
                        PRINT_INFO("[writeCali]: mkdir failed ERROR %s\n", tmp_path);
                        return -1;
                    } else {
                        PRINT_INFO("[writeCali]: mkdir success %s\n", tmp_path);
                    }
                }
            }
        }

        return 0;
    }
};
using CamBasePtr = std::shared_ptr<pico_cam::CamBase>;

#define PICO_CAMERA_IMPLEMENTATION()                                                                                   \
    Eigen::Vector2d distort(const Eigen::Vector2d &uv_norm) const override {                                           \
        return distortImpl(uv_norm);                                                                                   \
    }                                                                                                                  \
    Eigen::Vector2f distort(const Eigen::Vector2f &uv_norm) const override {                                           \
        return distortImpl(uv_norm);                                                                                   \
    }                                                                                                                  \
    Eigen::Vector2d distort(const Eigen::Vector2d &uv_norm, Eigen::Matrix2d &Juv2u) const override {                   \
        return distortImpl(uv_norm, &Juv2u);                                                                           \
    }                                                                                                                  \
    Eigen::Vector2f distort(const Eigen::Vector2f &uv_norm, Eigen::Matrix2f &Juv2u) const override {                   \
        return distortImpl(uv_norm, &Juv2u);                                                                           \
    }                                                                                                                  \
    Eigen::Vector2d undistort(const Eigen::Vector2d &uv_dist) const override {                                         \
        return undistortImpl(uv_dist);                                                                                 \
    }                                                                                                                  \
    Eigen::Vector2f undistort(const Eigen::Vector2f &uv_dist) const override {                                         \
        return undistortImpl(uv_dist);                                                                                 \
    }                                                                                                                  \
    Eigen::Vector2d distort(                                                                                           \
        const Eigen::Vector2d &uv_norm, Eigen::Matrix2d &Juv2u, Eigen::Ref<Eigen::MatrixXd> Juv2intr) const override { \
        return distortImpl(uv_norm, &Juv2u, &Juv2intr);                                                                \
    }                                                                                                                  \
    Eigen::Vector2f distort(                                                                                           \
        const Eigen::Vector2f &uv_norm, Eigen::Matrix2f &Juv2u, Eigen::Ref<Eigen::MatrixXf> Juv2intr) const override { \
        return distortImpl(uv_norm, &Juv2u, &Juv2intr);                                                                \
    }

}  // namespace pico_cam
