#include "realtime/render_profile.h"
#include "test_support.h"

int main() {
    const rt::RenderProfile quality = rt::RenderProfile::quality();
    expect_true(quality.samples_per_pixel >= 4, "quality spp");
    expect_true(quality.max_bounces >= 6, "quality bounce budget");
    expect_true(!quality.enable_denoise, "quality denoise off by default");

    const rt::RenderProfile balanced = rt::RenderProfile::balanced();
    expect_true(balanced.samples_per_pixel <= quality.samples_per_pixel, "balanced spp not above quality");
    expect_true(balanced.enable_denoise, "balanced denoise enabled");

    const rt::RenderProfile realtime = rt::RenderProfile::realtime();
    expect_true(realtime.samples_per_pixel <= balanced.samples_per_pixel, "realtime spp floor");
    expect_true(realtime.max_bounces <= balanced.max_bounces, "realtime lower bounce budget");
    return 0;
}
