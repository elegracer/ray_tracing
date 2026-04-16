#include "realtime/realtime_pipeline.h"
#include "realtime/render_profile.h"
#include "test_support.h"

int main() {
    rt::RealtimePipeline pipeline;
    const rt::RealtimeFrameSet first = pipeline.render_profiled_smoke_frame(2, rt::RenderProfile::balanced());
    expect_near(static_cast<double>(first.frames.size()), 2.0, 1e-12, "two active cameras");
    expect_true(first.frames[0].history_length == 1, "camera 0 history starts at 1");
    expect_true(first.frames[1].history_length == 1, "camera 1 history starts at 1");
    expect_true(first.frames[0].radiance.average_luminance > 0.01, "camera 0 radiance should be non-black");

    const rt::RealtimeFrameSet second = pipeline.render_profiled_smoke_frame(2, rt::RenderProfile::balanced());
    expect_true(second.frames[0].history_length == 2, "camera 0 accumulates");
    expect_true(second.frames[1].history_length == 2, "camera 1 accumulates");

    const rt::RealtimeFrameSet reset =
        pipeline.render_profiled_smoke_frame_with_pose_jump(2, rt::RenderProfile::balanced());
    expect_true(reset.frames[0].history_length == 1, "camera 0 reset");
    expect_true(reset.frames[1].history_length == 1, "camera 1 reset");
    return 0;
}
