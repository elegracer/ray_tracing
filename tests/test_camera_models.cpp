#include "realtime/camera_models.h"
#include "test_support.h"

#include <array>

int main() {
    using rt::Equi62Lut1DParams;
    using rt::Pinhole32Params;
    using rt::make_equi62_lut1d_params;
    using rt::project_equi62_lut1d;
    using rt::project_pinhole32;
    using rt::unproject_equi62_lut1d;
    using rt::unproject_pinhole32;

    const Pinhole32Params pinhole{
        320.0, 330.0, 160.0, 120.0,
        0.07, -0.03, 0.01, 0.002, -0.0015,
    };

    expect_vec3_near(unproject_pinhole32(pinhole, Eigen::Vector2d {160.0, 120.0}),
        Eigen::Vector3d {0.0, 0.0, 1.0}, 1e-12, "pinhole center unproject");
    expect_vec3_near(unproject_pinhole32(pinhole, project_pinhole32(pinhole, Eigen::Vector3d {0.23, -0.17, 1.0})),
        Eigen::Vector3d {0.23, -0.17, 1.0}.normalized(), 1e-9, "pinhole off-axis roundtrip");
    expect_true((project_pinhole32(pinhole, unproject_pinhole32(pinhole, Eigen::Vector2d {160.0, 120.0}))
                    - Eigen::Vector2d {160.0, 120.0})
                    .cwiseAbs()
                    .maxCoeff()
                < 1e-12,
        "pinhole center project roundtrip");

    const Equi62Lut1DParams equi = make_equi62_lut1d_params(640, 480, 280.0, 282.0, 320.0, 240.0,
        std::array<double, 6> {0.02, -0.005, 0.001, -0.0002, 0.00005, -0.00001},
        Eigen::Vector2d {0.0012, -0.0009});

    expect_vec3_near(unproject_equi62_lut1d(equi, Eigen::Vector2d {320.0, 240.0}),
        Eigen::Vector3d {0.0, 0.0, 1.0}, 1e-12, "equi center unproject");
    expect_vec3_near(unproject_equi62_lut1d(equi, project_equi62_lut1d(equi, Eigen::Vector3d {0.21, 0.08, 1.0})),
        Eigen::Vector3d {0.21, 0.08, 1.0}.normalized(), 2e-6, "equi off-axis roundtrip");

    const Equi62Lut1DParams equi_identity = make_equi62_lut1d_params(640, 480, 280.0, 280.0, 320.0, 240.0,
        std::array<double, 6> {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, Eigen::Vector2d {0.0, 0.0});
    const double theta = 0.3;
    expect_vec3_near(unproject_equi62_lut1d(equi_identity, Eigen::Vector2d {320.0 + 280.0 * theta, 240.0}),
        Eigen::Vector3d {std::sin(theta), 0.0, std::cos(theta)}, 1e-12, "equi analytic off-axis unproject");

    const Eigen::Vector2d lut_out_of_range_pixel {320.0 + 280.0 * 5.0, 240.0 - 280.0 * 1.5};
    expect_vec3_near(unproject_equi62_lut1d(equi_identity, lut_out_of_range_pixel),
        Eigen::Vector3d {5.0, -1.5, 1.0}.normalized(), 1e-12, "equi lut out-of-range fallback");

    return 0;
}
