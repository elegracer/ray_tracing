#include "realtime/render_profile.h"
#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/default_viewer_scene.h"
// Task 2 keeps this include red on purpose until Task 3 adds the rig builder.
#include "realtime/viewer/four_camera_rig.h"
#include "test_support.h"

int main() {
    const rt::SceneDescription scene = rt::viewer::make_default_viewer_scene();
    const rt::PackedScene packed_scene = scene.pack();
    expect_true(packed_scene.material_count >= 5, "final_room materials");
    expect_true(packed_scene.sphere_count >= 6, "final_room spheres");
    expect_true(packed_scene.quad_count >= 7, "final_room quads");

    const rt::RenderProfile profile = rt::viewer::default_viewer_profile();
    expect_true(profile.samples_per_pixel == 1, "viewer spp");
    expect_true(profile.max_bounces == 2, "viewer bounces");
    expect_true(!profile.enable_denoise, "viewer denoise disabled");
    return 0;
}
