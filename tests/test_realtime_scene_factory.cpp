#include "realtime/realtime_scene_factory.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "test_support.h"

int main() {
    const rt::PackedScene smoke = rt::make_realtime_scene("smoke").pack();
    expect_true(smoke.sphere_count >= 1, "smoke scene has geometry");

    const rt::PackedScene final_room = rt::make_realtime_scene("final_room").pack();
    expect_true(final_room.quad_count >= 7, "final_room quads");

    expect_true(rt::realtime_scene_supported("smoke"), "smoke supported");
    expect_true(!rt::realtime_scene_supported("cornell_box"), "cornell_box unsupported");

    const rt::PackedScene default_view = rt::viewer::make_default_viewer_scene().pack();
    expect_true(default_view.quad_count == final_room.quad_count, "default viewer scene is final_room");
    return 0;
}
