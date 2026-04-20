#include "common/camera.h"
#include "core/offline_shared_scene_renderer.h"
#include "realtime/camera_models.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

#include <Eigen/Geometry>
#include <opencv2/core.hpp>
#include <tbb/global_control.h>

namespace {

rt::PackedCamera make_test_pinhole_camera() {
    rt::PackedCamera camera;
    camera.width = 64;
    camera.height = 48;
    camera.model = rt::CameraModelType::pinhole32;
    camera.T_rc = Sophus::SE3d(Eigen::AngleAxisd(0.35, Eigen::Vector3d::UnitY()).toRotationMatrix(),
        Eigen::Vector3d(1.0, -2.0, 3.5));
    camera.pinhole = rt::Pinhole32Params {
        52.0, 50.0, 31.5, 23.5,
        0.02, -0.01, 0.003, 0.001, -0.0015,
    };
    return camera;
}

rt::PackedCamera make_test_equi_camera() {
    rt::PackedCamera camera;
    camera.width = 64;
    camera.height = 48;
    camera.model = rt::CameraModelType::equi62_lut1d;
    camera.T_rc = Sophus::SE3d(Eigen::AngleAxisd(-0.2, Eigen::Vector3d::UnitX()).toRotationMatrix(),
        Eigen::Vector3d(-1.5, 0.75, 2.25));
    camera.equi = rt::make_equi62_lut1d_params(camera.width, camera.height, 28.0, 29.0, 31.5, 23.5,
        std::array<double, 6> {0.01, -0.003, 0.0008, -0.0002, 0.00005, -0.00001},
        Eigen::Vector2d {0.0007, -0.0005});
    return camera;
}

Camera::SharedCameraRayConfig make_camera_ray_config(const rt::PackedCamera& packed) {
    Camera::SharedCameraRayConfig config {};
    config.model = packed.model;
    config.origin = packed.T_rc.translation();
    config.camera_to_world = packed.T_rc.rotationMatrix();
    if (packed.model == rt::CameraModelType::pinhole32) {
        config.pinhole = packed.pinhole;
    } else {
        config.equi = packed.equi;
    }
    return config;
}

}  // namespace

int main() {
    tbb::global_control render_threads(tbb::global_control::max_allowed_parallelism, 1);

    const rt::scene::CpuRenderPreset* cornell_default =
        rt::scene::default_cpu_render_preset("cornell_box");
    expect_true(cornell_default != nullptr, "cornell_box default preset exists");
    expect_true(cornell_default->camera.camera.model == rt::CameraModelType::pinhole32,
        "cornell preset carries pinhole model");
    expect_true(cornell_default->camera.camera.width == 1280, "cornell preset camera width");
    expect_true(cornell_default->camera.camera.height == 720, "cornell preset camera height");
    expect_true(cornell_default->camera.camera.fx > 0.0, "cornell preset fx populated");
    expect_true(cornell_default->camera.camera.fy > 0.0, "cornell preset fy populated");
    expect_near(cornell_default->camera.camera.pinhole32.k1, 0.0, 1e-12, "cornell preset default k1");
    expect_vec3_near(cornell_default->camera.lookfrom, Eigen::Vector3d(278.0, 278.0, -800.0), 1e-12,
        "cornell lookfrom preserved");

    const rt::scene::CpuRenderPreset* rttnw_extreme =
        rt::scene::find_cpu_render_preset("rttnw_final_scene", "extreme");
    expect_true(rttnw_extreme != nullptr, "rttnw extreme preset exists");
    expect_true(rttnw_extreme->samples_per_pixel == 10000, "rttnw extreme spp preserved");

    const rt::scene::CpuRenderPreset* imported_default =
        rt::scene::default_cpu_render_preset("imported_obj_smoke");
    expect_true(imported_default != nullptr, "file-backed default preset exists");
    expect_true(imported_default->samples_per_pixel == 64, "file-backed default spp preserved");

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

    const cv::Mat imported_image = rt::render_shared_scene("imported_obj_smoke", 1);
    expect_true(!imported_image.empty(), "file-backed offline render should return a non-empty image");
    expect_true(imported_image.cols == 640, "file-backed offline render width follows yaml preset");

    const rt::PackedCamera pinhole_camera = make_test_pinhole_camera();
    Camera pinhole_seam;
    pinhole_seam.aspect_ratio =
        static_cast<double>(pinhole_camera.width) / static_cast<double>(pinhole_camera.height);
    pinhole_seam.image_width = pinhole_camera.width;
    pinhole_seam.samples_per_pixel = 1;
    pinhole_seam.defocus_angle = 0.0;
    pinhole_seam.focus_dist = 3.0;
    pinhole_seam.set_shared_camera_ray_config(make_camera_ray_config(pinhole_camera));
    const Eigen::Vector2d pinhole_pixel {17.5, 11.5};
    const Ray pinhole_ray = pinhole_seam.debug_primary_ray(pinhole_pixel);
    expect_vec3_near(pinhole_ray.origin(), pinhole_camera.T_rc.translation(), 1e-12,
        "pinhole seam origin follows packed pose");
    expect_vec3_near(pinhole_ray.direction().normalized(),
        (pinhole_camera.T_rc.rotationMatrix()
            * rt::unproject_pinhole32(pinhole_camera.pinhole, pinhole_pixel))
            .normalized(),
        1e-12, "pinhole seam ray direction matches shared camera math");

    const rt::PackedCamera equi_camera = make_test_equi_camera();
    Camera equi_seam;
    equi_seam.aspect_ratio = static_cast<double>(equi_camera.width) / static_cast<double>(equi_camera.height);
    equi_seam.image_width = equi_camera.width;
    equi_seam.samples_per_pixel = 1;
    equi_seam.defocus_angle = 2.0;
    equi_seam.focus_dist = 3.0;
    equi_seam.set_shared_camera_ray_config(make_camera_ray_config(equi_camera));
    const Eigen::Vector2d equi_pixel {45.5, 14.5};
    const Ray equi_ray = equi_seam.debug_primary_ray(equi_pixel);
    expect_vec3_near(equi_ray.origin(), equi_camera.T_rc.translation(), 1e-12,
        "equi seam origin stays at camera center");
    expect_vec3_near(equi_ray.direction().normalized(),
        (equi_camera.T_rc.rotationMatrix()
            * rt::unproject_equi62_lut1d(equi_camera.equi, equi_pixel))
            .normalized(),
        1e-9, "equi seam ray direction matches shared camera math");

    Camera pinhole_defocus_seam;
    pinhole_defocus_seam.aspect_ratio =
        static_cast<double>(pinhole_camera.width) / static_cast<double>(pinhole_camera.height);
    pinhole_defocus_seam.image_width = pinhole_camera.width;
    pinhole_defocus_seam.samples_per_pixel = 1;
    pinhole_defocus_seam.defocus_angle = 1.5;
    pinhole_defocus_seam.focus_dist = 4.0;
    pinhole_defocus_seam.set_shared_camera_ray_config(make_camera_ray_config(pinhole_camera));
    bool pinhole_used_defocus = false;
    for (int i = 0; i < 8; ++i) {
        const Ray defocused_ray = pinhole_defocus_seam.debug_primary_ray(pinhole_pixel);
        if ((defocused_ray.origin() - pinhole_camera.T_rc.translation()).norm() > 1e-12) {
            pinhole_used_defocus = true;
            break;
        }
    }
    expect_true(pinhole_used_defocus, "pinhole seam keeps depth of field enabled");

    bool equi_used_defocus = false;
    for (int i = 0; i < 4; ++i) {
        const Ray maybe_defocused_ray = equi_seam.debug_primary_ray(equi_pixel);
        if ((maybe_defocused_ray.origin() - equi_camera.T_rc.translation()).norm() > 1e-12) {
            equi_used_defocus = true;
            break;
        }
    }
    expect_true(!equi_used_defocus, "equi seam stays no-defocus");

    const cv::Mat pinhole_reference = rt::render_shared_scene_from_camera("quads", pinhole_camera, 1);
    const cv::Mat equi_reference = rt::render_shared_scene_from_camera("quads", equi_camera, 1);
    expect_true(!pinhole_reference.empty(), "pinhole packed-camera offline render should succeed");
    expect_true(!equi_reference.empty(), "equi packed-camera offline render should succeed");
    expect_true(pinhole_reference.cols == pinhole_camera.width && pinhole_reference.rows == pinhole_camera.height,
        "pinhole packed-camera render keeps camera dimensions");
    expect_true(equi_reference.cols == equi_camera.width && equi_reference.rows == equi_camera.height,
        "equi packed-camera render keeps full rectangular dimensions");
    expect_true(cv::norm(pinhole_reference, equi_reference, cv::NORM_L1) > 0.0,
        "switching the offline shared-scene camera model changes the rendered result");
    return 0;
}
