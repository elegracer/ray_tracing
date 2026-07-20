#pragma once

#include <optional>
#include <string>

namespace rt {

struct RenderProfile {
    int samples_per_pixel = 1;
    int max_bounces = 4;
    bool enable_denoise = true;
    int rr_start_bounce = 3;
    double accumulation_reset_rotation_deg = 2.0;
    double accumulation_reset_translation = 0.05;
    bool enable_restir_di = false;
    int restir_initial_candidates = 4;
    bool restir_temporal_reuse = true;
    int restir_max_history_age = 20;
    int restir_max_temporal_candidates = 64;
    int restir_min_analytic_lights = 16;

    static RenderProfile quality() {
        return RenderProfile{
            .samples_per_pixel = 4,
            .max_bounces = 8,
            .enable_denoise = false,
            .rr_start_bounce = 6,
            .accumulation_reset_rotation_deg = 0.5,
            .accumulation_reset_translation = 0.01,
            .enable_restir_di = false,
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
            .enable_restir_di = true,
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
            .enable_restir_di = true,
        };
    }

    static RenderProfile realtime_default() { return balanced(); }
};

std::optional<RenderProfile> render_profile_from_name(const std::string& profile_name);
std::string render_profile_name(const RenderProfile& profile);

}  // namespace rt
