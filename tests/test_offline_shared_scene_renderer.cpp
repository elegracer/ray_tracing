#include "core/offline_shared_scene_renderer.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

#include <opencv2/core.hpp>
#include <tbb/global_control.h>

int main() {
    tbb::global_control render_threads(tbb::global_control::max_allowed_parallelism, 1);

    const rt::scene::CpuRenderPreset* cornell_default =
        rt::scene::default_cpu_render_preset("cornell_box");
    expect_true(cornell_default != nullptr, "cornell_box default preset exists");
    expect_near(cornell_default->camera.vfov, 40.0, 1e-12, "cornell vfov preserved");
    expect_vec3_near(cornell_default->camera.lookfrom, Eigen::Vector3d(278.0, 278.0, -800.0), 1e-12,
        "cornell lookfrom preserved");

    const rt::scene::CpuRenderPreset* rttnw_extreme =
        rt::scene::find_cpu_render_preset("rttnw_final_scene", "extreme");
    expect_true(rttnw_extreme != nullptr, "rttnw extreme preset exists");
    expect_true(rttnw_extreme->samples_per_pixel == 10000, "rttnw extreme spp preserved");

    const cv::Mat image = rt::render_shared_scene("quads", 1);
    expect_true(!image.empty(), "offline shared-scene render should return a non-empty image");
    expect_true(image.data != nullptr, "offline shared-scene render should populate image data");
    expect_true(image.rows > 0 && image.cols > 0, "offline shared-scene render should have positive dimensions");
    expect_true(image.rows >= 360 && image.cols >= 640,
        "offline shared-scene render should produce plausible image dimensions");
    expect_true(image.type() == CV_8UC3, "offline shared-scene render should return CV_8UC3 output");

    const double aspect_ratio = static_cast<double>(image.cols) / static_cast<double>(image.rows);
    expect_true(aspect_ratio > 1.5 && aspect_ratio < 2.1,
        "offline shared-scene render should produce a plausible widescreen aspect ratio");
    return 0;
}
