#include "core/offline_shared_scene_renderer.h"
#include "realtime/camera_models.h"
#include "realtime/realtime_scene_factory.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "test_support.h"

#include <opencv2/opencv.hpp>
#include <tbb/global_control.h>

#include <cmath>

namespace {

double compute_cpu_mean_luminance(const cv::Mat& image) {
    expect_true(!image.empty(), "cpu image is present");
    expect_true(image.type() == CV_8UC3, "cpu image uses CV_8UC3");

    double sum = 0.0;
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
            const cv::Vec3b pixel = image.at<cv::Vec3b>(y, x);
            sum += (pixel[0] + pixel[1] + pixel[2]) / (3.0 * 255.0);
        }
    }
    return sum / static_cast<double>(image.rows * image.cols);
}

rt::PackedCameraRig make_single_camera_rig(const rt::PackedCamera& camera) {
    rt::PackedCameraRig rig {};
    rig.active_count = 1;
    rig.cameras[0] = camera;
    rig.cameras[0].enabled = 1;
    return rig;
}

rt::RenderProfile make_smoke_profile() {
    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 4;
    profile.max_bounces = 4;
    return profile;
}

rt::PackedCamera make_helper_derived_equi_camera(std::string_view scene_id, int width, int height) {
    rt::PackedCamera camera = rt::default_camera_rig_for_scene(scene_id, 1, width, height).pack().cameras[0];
    const rt::DefaultCameraIntrinsics intrinsics = rt::derive_default_camera_intrinsics(
        rt::CameraModelType::equi62_lut1d, width, height, rt::default_hfov_deg(rt::CameraModelType::equi62_lut1d));
    camera.model = rt::CameraModelType::equi62_lut1d;
    camera.width = width;
    camera.height = height;
    camera.equi = rt::make_equi62_lut1d_params(width, height, intrinsics.fx, intrinsics.fy, intrinsics.cx, intrinsics.cy,
        std::array<double, 6> {}, Eigen::Vector2d::Zero());
    return camera;
}

void expect_cpu_gpu_smoke(std::string_view scene_id, const rt::PackedCamera& camera, const std::string& label) {
    const cv::Mat cpu = rt::render_shared_scene_from_camera(scene_id, camera, 4);
    expect_true(!cpu.empty(), label + " cpu frame present");
    expect_true(cpu.cols == camera.width && cpu.rows == camera.height, label + " cpu dimensions preserved");

    rt::OptixRenderer renderer;
    const rt::RadianceFrame gpu = renderer.render_radiance(
        rt::make_realtime_scene(scene_id).pack(), make_single_camera_rig(camera), make_smoke_profile(), 0);

    expect_true(gpu.width == camera.width, label + " gpu width preserved");
    expect_true(gpu.height == camera.height, label + " gpu height preserved");
    expect_true(gpu.average_luminance > 0.001, label + " gpu frame is non-black");
    expect_true(!gpu.beauty_rgba.empty(), label + " gpu beauty buffer present");
    expect_true(!gpu.normal_rgba.empty(), label + " gpu normal buffer present");
    expect_true(!gpu.albedo_rgba.empty(), label + " gpu albedo buffer present");
    expect_true(!gpu.depth.empty(), label + " gpu depth buffer present");

    const double cpu_mean_luminance = compute_cpu_mean_luminance(cpu);
    expect_true(cpu_mean_luminance > 0.001, label + " cpu frame is non-black");
    expect_true(std::abs(gpu.average_luminance - cpu_mean_luminance) < 0.12,
        label + " cpu and gpu mean luminance stay within smoke tolerance");
}

}  // namespace

int main() {
    tbb::global_control render_threads(tbb::global_control::max_allowed_parallelism, 1);

    constexpr int kWidth = 64;
    constexpr int kHeight = 48;

    const rt::PackedCamera explicit_pinhole =
        rt::default_camera_rig_for_scene("final_room", 1, kWidth, kHeight).pack().cameras[0];
    expect_true(explicit_pinhole.model == rt::CameraModelType::pinhole32,
        "explicit authored pinhole scene stays pinhole");
    expect_cpu_gpu_smoke("final_room", explicit_pinhole, "explicit pinhole smoke");

    const rt::PackedCamera helper_equi = make_helper_derived_equi_camera("quads", kWidth, kHeight);
    expect_true(helper_equi.model == rt::CameraModelType::equi62_lut1d,
        "helper-derived default camera uses equi");
    expect_cpu_gpu_smoke("quads", helper_equi, "helper-derived equi smoke");

    return 0;
}
