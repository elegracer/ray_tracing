#pragma once

namespace rt {

struct RenderProfile {
    int samples_per_pixel = 1;
    int max_bounces = 4;
    bool enable_denoise = true;
    int rr_start_bounce = 3;
    double accumulation_reset_rotation_deg = 2.0;
    double accumulation_reset_translation = 0.05;

    static RenderProfile quality() {
        return RenderProfile{
            .samples_per_pixel = 4,
            .max_bounces = 8,
            .enable_denoise = false,
            .rr_start_bounce = 6,
            .accumulation_reset_rotation_deg = 0.5,
            .accumulation_reset_translation = 0.01,
        };
    }

    static RenderProfile balanced() {
        return RenderProfile{
            .samples_per_pixel = 2,
            .max_bounces = 4,
            .enable_denoise = true,
            .rr_start_bounce = 3,
            .accumulation_reset_rotation_deg = 1.0,
            .accumulation_reset_translation = 0.02,
        };
    }

    static RenderProfile realtime() {
        return RenderProfile{
            .samples_per_pixel = 1,
            .max_bounces = 2,
            .enable_denoise = true,
            .rr_start_bounce = 2,
            .accumulation_reset_rotation_deg = 2.0,
            .accumulation_reset_translation = 0.05,
        };
    }

    static RenderProfile realtime_default() { return balanced(); }
};

}  // namespace rt
