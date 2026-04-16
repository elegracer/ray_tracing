#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "test_support.h"

int main() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {320.0, 320.0, 160.0, 120.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 320, 240);

    rt::OptixRenderer renderer;
    const rt::DirectionDebugFrame frame = renderer.render_direction_debug(rig.pack());

    expect_near(static_cast<double>(frame.width), 320.0, 1e-12, "debug width");
    expect_near(static_cast<double>(frame.height), 240.0, 1e-12, "debug height");
    expect_true(!frame.rgba.empty(), "debug image has pixels");
    return 0;
}
