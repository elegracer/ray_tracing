#include "realtime/frame_convention.h"
#include "test_support.h"

int main() {
    const Vec3d camera_x = camera_to_renderer(Vec3d {1.0, 0.0, 0.0});
    const Vec3d camera_y = camera_to_renderer(Vec3d {0.0, 1.0, 0.0});
    const Vec3d camera_z = camera_to_renderer(Vec3d {0.0, 0.0, 1.0});
    const Vec3d body_x = body_to_renderer(Vec3d {1.0, 0.0, 0.0});
    const Vec3d body_y = body_to_renderer(Vec3d {0.0, 1.0, 0.0});
    const Vec3d body_z = body_to_renderer(Vec3d {0.0, 0.0, 1.0});

    expect_vec3_near(camera_x, Vec3d {1.0, 0.0, 0.0});
    expect_vec3_near(camera_y, Vec3d {0.0, -1.0, 0.0});
    expect_vec3_near(camera_z, Vec3d {0.0, 0.0, -1.0});
    expect_vec3_near(body_x, Vec3d {0.0, 1.0, 0.0});
    expect_vec3_near(body_y, Vec3d {-1.0, 0.0, 0.0});
    expect_vec3_near(body_z, Vec3d {0.0, 0.0, 1.0});

    return 0;
}
