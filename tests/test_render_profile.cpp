#include "realtime/render_profile.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "test_support.h"

#include <optional>
#include <string>

int main() {
    const rt::RenderProfile quality = rt::RenderProfile::quality();
    expect_true(quality.samples_per_pixel == 4, "quality spp");
    expect_true(quality.max_bounces == 8, "quality bounce budget");
    expect_true(!quality.enable_denoise, "quality denoise off by default");
    expect_true(quality.rr_start_bounce == 6, "quality rr start");
    expect_true(quality.accumulation_reset_rotation_deg == 0.5, "quality accumulation rotation");
    expect_true(quality.accumulation_reset_translation == 0.01, "quality accumulation translation");

    const rt::RenderProfile balanced = rt::RenderProfile::balanced();
    expect_true(balanced.samples_per_pixel == 2, "balanced spp");
    expect_true(balanced.max_bounces == 4, "balanced bounce budget");
    expect_true(balanced.enable_denoise, "balanced denoise enabled");
    expect_true(balanced.rr_start_bounce == 3, "balanced rr start");
    expect_true(balanced.accumulation_reset_rotation_deg == 1.0, "balanced accumulation rotation");
    expect_true(balanced.accumulation_reset_translation == 0.02, "balanced accumulation translation");

    const rt::RenderProfile realtime = rt::RenderProfile::realtime();
    expect_true(realtime.samples_per_pixel == 1, "realtime spp");
    expect_true(realtime.max_bounces == 2, "realtime bounce budget");
    expect_true(realtime.enable_denoise, "realtime denoise enabled");
    expect_true(realtime.rr_start_bounce == 2, "realtime rr start");
    expect_true(realtime.accumulation_reset_rotation_deg == 2.0, "realtime accumulation rotation");
    expect_true(realtime.accumulation_reset_translation == 0.05, "realtime accumulation translation");

    const rt::RenderProfile realtime_default = rt::RenderProfile::realtime_default();
    expect_true(realtime_default.samples_per_pixel == balanced.samples_per_pixel, "default spp matches balanced");
    expect_true(realtime_default.max_bounces == balanced.max_bounces, "default bounces match balanced");
    expect_true(realtime_default.enable_denoise == balanced.enable_denoise, "default denoise matches balanced");
    expect_true(realtime_default.rr_start_bounce == balanced.rr_start_bounce, "default rr start matches balanced");
    expect_true(realtime_default.accumulation_reset_rotation_deg == balanced.accumulation_reset_rotation_deg, "default rotation matches balanced");
    expect_true(realtime_default.accumulation_reset_translation == balanced.accumulation_reset_translation, "default translation matches balanced");

    const rt::RenderProfile viewer_preview = rt::viewer::default_viewer_preview_profile();
    expect_true(viewer_preview.samples_per_pixel == 1, "viewer preview spp");
    expect_true(viewer_preview.max_bounces == 4, "viewer preview bounce budget");
    expect_true(!viewer_preview.enable_denoise, "viewer preview denoise disabled");
    expect_true(viewer_preview.rr_start_bounce == 2, "viewer preview rr start");
    expect_true(viewer_preview.accumulation_reset_rotation_deg == 2.0, "viewer preview accumulation rotation");
    expect_true(viewer_preview.accumulation_reset_translation == 0.05, "viewer preview accumulation translation");

    const rt::RenderProfile viewer_converge = rt::viewer::default_viewer_converge_profile();
    expect_true(viewer_converge.samples_per_pixel == 2, "viewer converge spp");
    expect_true(viewer_converge.max_bounces == 4, "viewer converge bounce budget");
    expect_true(!viewer_converge.enable_denoise, "viewer converge denoise disabled");
    expect_true(viewer_converge.rr_start_bounce == 3, "viewer converge rr start");
    expect_true(viewer_converge.accumulation_reset_rotation_deg == 2.0, "viewer converge accumulation rotation");
    expect_true(viewer_converge.accumulation_reset_translation == 0.05, "viewer converge accumulation translation");

    const rt::RenderProfile viewer_default = rt::viewer::default_viewer_profile();
    expect_true(viewer_default.samples_per_pixel == viewer_preview.samples_per_pixel, "viewer default spp matches preview");
    expect_true(viewer_default.max_bounces == viewer_preview.max_bounces, "viewer default bounces match preview");
    expect_true(viewer_default.enable_denoise == viewer_preview.enable_denoise, "viewer default denoise matches preview");
    expect_true(viewer_default.rr_start_bounce == viewer_preview.rr_start_bounce, "viewer default rr start matches preview");
    expect_true(viewer_default.accumulation_reset_rotation_deg == viewer_preview.accumulation_reset_rotation_deg, "viewer default rotation matches preview");
    expect_true(viewer_default.accumulation_reset_translation == viewer_preview.accumulation_reset_translation, "viewer default translation matches preview");

    const std::optional<rt::RenderProfile> named_quality = rt::render_profile_from_name("quality");
    expect_true(named_quality.has_value(), "quality profile resolved");
    expect_true(named_quality->samples_per_pixel == quality.samples_per_pixel, "quality profile spp resolved");
    const std::optional<rt::RenderProfile> named_balanced = rt::render_profile_from_name("balanced");
    expect_true(named_balanced.has_value(), "balanced profile resolved");
    expect_true(named_balanced->samples_per_pixel == balanced.samples_per_pixel, "balanced profile spp resolved");
    const std::optional<rt::RenderProfile> named_realtime = rt::render_profile_from_name("realtime");
    expect_true(named_realtime.has_value(), "realtime profile resolved");
    expect_true(named_realtime->samples_per_pixel == realtime.samples_per_pixel, "realtime profile spp resolved");
    expect_true(!rt::render_profile_from_name("bad").has_value(), "unknown profile rejected");
    expect_true(rt::render_profile_name(rt::RenderProfile::realtime_default()) == std::string("balanced"),
        "default profile name");
    expect_true(rt::render_profile_name(rt::RenderProfile {}) == std::string("default"), "custom profile name");
    return 0;
}
