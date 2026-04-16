#include "realtime/gpu/denoiser.h"
#include "realtime/realtime_pipeline.h"
#include "realtime/render_profile.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>

namespace {

double compute_average_luminance_from_beauty(const std::vector<float>& rgba) {
    double sum = 0.0;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const double r = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 0]))), 0.0, 0.999);
        const double g = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 1]))), 0.0, 0.999);
        const double b = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 2]))), 0.0, 0.999);
        sum += (r + g + b) / 3.0;
    }
    return rgba.empty() ? 0.0 : sum / static_cast<double>(rgba.size() / 4);
}

}  // namespace

int main() {
    rt::RadianceFrame denoiser_probe {};
    denoiser_probe.width = 1;
    denoiser_probe.height = 1;
    denoiser_probe.average_luminance = 0.0;
    denoiser_probe.beauty_rgba = {0.25f, 0.09f, 0.04f, 1.0f};
    denoiser_probe.albedo_rgba = {0.0f, 0.0f, 0.0f, 1.0f};
    denoiser_probe.normal_rgba = {0.0f, 0.0f, 1.0f, 0.0f};
    rt::OptixDenoiserWrapper denoiser;
    denoiser.run(denoiser_probe);
    const double probe_expected = compute_average_luminance_from_beauty(denoiser_probe.beauty_rgba);
    expect_near(denoiser_probe.average_luminance, probe_expected, 1e-12, "denoiser keeps average_luminance in sync");

    rt::RealtimePipeline pipeline;
    const rt::RealtimeFrameSet first = pipeline.render_profiled_smoke_frame(2, rt::RenderProfile::balanced());
    expect_near(static_cast<double>(first.frames.size()), 2.0, 1e-12, "two active cameras");
    expect_true(first.frames[0].history_length == 1, "camera 0 history starts at 1");
    expect_true(first.frames[1].history_length == 1, "camera 1 history starts at 1");
    expect_true(first.frames[0].radiance.average_luminance > 0.01, "camera 0 radiance should be non-black");
    const double first_expected_luminance =
        compute_average_luminance_from_beauty(first.frames[0].radiance.beauty_rgba);
    expect_near(first.frames[0].radiance.average_luminance, first_expected_luminance, 1e-9,
        "camera 0 average_luminance matches final beauty");

    const rt::RealtimeFrameSet second = pipeline.render_profiled_smoke_frame(2, rt::RenderProfile::balanced());
    expect_true(second.frames[0].history_length == 2, "camera 0 accumulates");
    expect_true(second.frames[1].history_length == 2, "camera 1 accumulates");

    const rt::RealtimeFrameSet reset =
        pipeline.render_profiled_smoke_frame_with_pose_jump(2, rt::RenderProfile::balanced());
    expect_true(reset.frames[0].history_length == 1, "camera 0 reset");
    expect_true(reset.frames[1].history_length == 1, "camera 1 reset");

    const rt::RealtimeFrameSet after_jump = pipeline.render_profiled_smoke_frame(2, rt::RenderProfile::balanced());
    expect_true(after_jump.frames[0].history_length == 2, "camera 0 resumes accumulation after jump");
    expect_true(after_jump.frames[1].history_length == 2, "camera 1 resumes accumulation after jump");
    return 0;
}
