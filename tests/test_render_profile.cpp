#include "realtime/render_profile.h"
#include "test_support.h"

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
    return 0;
}
