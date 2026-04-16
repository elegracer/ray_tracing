#pragma once

namespace rt {

struct RenderProfile {
    int samples_per_pixel = 1;
    int max_bounces = 4;
    bool enable_denoise = true;
    int rr_start_bounce = 3;
    double accumulation_reset_rotation_deg = 2.0;
    double accumulation_reset_translation = 0.05;

    static RenderProfile realtime_default() {
        return RenderProfile {};
    }
};

}  // namespace rt
