#include "realtime/render_profile.h"

namespace rt {
namespace {

bool render_profiles_equal(const RenderProfile& lhs, const RenderProfile& rhs) {
    return lhs.samples_per_pixel == rhs.samples_per_pixel && lhs.max_bounces == rhs.max_bounces
        && lhs.enable_denoise == rhs.enable_denoise && lhs.rr_start_bounce == rhs.rr_start_bounce
        && lhs.accumulation_reset_rotation_deg == rhs.accumulation_reset_rotation_deg
        && lhs.accumulation_reset_translation == rhs.accumulation_reset_translation;
}

}  // namespace

std::optional<RenderProfile> render_profile_from_name(const std::string& profile_name) {
    if (profile_name == "quality") {
        return RenderProfile::quality();
    }
    if (profile_name == "balanced") {
        return RenderProfile::balanced();
    }
    if (profile_name == "realtime") {
        return RenderProfile::realtime();
    }
    return std::nullopt;
}

std::string render_profile_name(const RenderProfile& profile) {
    if (render_profiles_equal(profile, RenderProfile::quality())) {
        return "quality";
    }
    if (render_profiles_equal(profile, RenderProfile::balanced())) {
        return "balanced";
    }
    if (render_profiles_equal(profile, RenderProfile::realtime())) {
        return "realtime";
    }
    return "default";
}

}  // namespace rt
