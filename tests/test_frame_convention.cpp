#include "realtime/frame_convention.h"
#include "test_support.h"

int main() {
    using rt::body_to_world;
    using rt::camera_to_world;

    expect_vec3_near(camera_to_world(Eigen::Vector3d {1.0, 0.0, 0.0}),
        Eigen::Vector3d {1.0, 0.0, 0.0}, 1e-12, "camera x axis");
    expect_vec3_near(camera_to_world(Eigen::Vector3d {0.0, 1.0, 0.0}),
        Eigen::Vector3d {0.0, 0.0, -1.0}, 1e-12, "camera y axis");
    expect_vec3_near(camera_to_world(Eigen::Vector3d {0.0, 0.0, 1.0}),
        Eigen::Vector3d {0.0, 1.0, 0.0}, 1e-12, "camera z axis");

    expect_vec3_near(body_to_world(Eigen::Vector3d {1.0, 0.0, 0.0}),
        Eigen::Vector3d {0.0, 0.0, 1.0}, 1e-12, "body x axis");
    expect_vec3_near(body_to_world(Eigen::Vector3d {0.0, 1.0, 0.0}),
        Eigen::Vector3d {-1.0, 0.0, 0.0}, 1e-12, "body y axis");
    expect_vec3_near(body_to_world(Eigen::Vector3d {0.0, 0.0, 1.0}),
        Eigen::Vector3d {0.0, -1.0, 0.0}, 1e-12, "body z axis");

    return 0;
}
