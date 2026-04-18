#include "realtime/viewer/default_viewer_scene.h"

#include "realtime/realtime_scene_factory.h"

namespace rt::viewer {

SceneDescription make_final_room_scene() {
    return rt::make_realtime_scene("final_room");
}

SceneDescription make_default_viewer_scene() {
    return make_final_room_scene();
}

RenderProfile default_viewer_profile() {
    return RenderProfile {
        .samples_per_pixel = 1,
        .max_bounces = 2,
        .enable_denoise = false,
        .rr_start_bounce = 2,
        .accumulation_reset_rotation_deg = 2.0,
        .accumulation_reset_translation = 0.05,
    };
}

}  // namespace rt::viewer
