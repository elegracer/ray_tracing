#include "camera_contract_fixtures.h"
#include "common/camera.h"
#include "core/offline_shared_scene_renderer.h"
#include "realtime/camera_models.h"
#include "scene/scene_file_catalog.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

#include <Eigen/Geometry>
#include <filesystem>
#include <fstream>
#include <opencv2/core.hpp>
#include <string_view>
#include <tbb/global_control.h>

namespace {

namespace fs = std::filesystem;

void write_text_file(const fs::path& path, std::string_view contents) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << contents;
}

void write_shared_scene_model_switch_scene(
    const fs::path& scene_file, std::string_view scene_id, std::string_view model) {
    fs::create_directories(scene_file.parent_path());
    const bool pinhole = model == "pinhole32";
    const double focal = pinhole ? 55.0 : 24.0;
    const std::string model_specific = pinhole
        ? R"(      pinhole32:
        k1: 0.0
        k2: 0.0
        k3: 0.0
        p1: 0.0
        p2: 0.0
)"
        : R"(      equi62_lut1d:
        radial: [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        tangential: [0.0, 0.0]
)";

    std::ofstream out(scene_file);
    out << "format_version: 1\n"
           "scene:\n"
           "  id: " << scene_id << "\n"
           "  label: Model Switch Scene\n"
           "  background: [0.2, 0.3, 0.5]\n"
           "  textures:\n"
           "    white:\n"
           "      type: constant\n"
           "      color: [0.8, 0.8, 0.8]\n"
           "    red:\n"
           "      type: constant\n"
           "      color: [0.9, 0.2, 0.2]\n"
           "  materials:\n"
           "    matte_white:\n"
           "      type: diffuse\n"
           "      albedo: white\n"
           "    matte_red:\n"
           "      type: diffuse\n"
           "      albedo: red\n"
           "  shapes:\n"
           "    center_ball:\n"
           "      type: sphere\n"
           "      center: [0.0, 0.0, 0.0]\n"
           "      radius: 0.6\n"
           "    side_ball:\n"
           "      type: sphere\n"
           "      center: [1.1, 0.2, 1.0]\n"
           "      radius: 0.4\n"
           "  instances:\n"
           "    - shape: center_ball\n"
           "      material: matte_white\n"
           "    - shape: side_ball\n"
           "      material: matte_red\n"
           "cpu_presets:\n"
           "  default:\n"
           "    samples_per_pixel: 1\n"
           "    camera:\n"
           "      model: " << model << "\n"
           "      width: 64\n"
           "      height: 48\n"
           "      fx: " << focal << "\n"
           "      fy: " << focal << "\n"
           "      cx: 32.0\n"
           "      cy: 24.0\n"
           "      T_bc:\n"
           "        translation: [0.0, 0.0, 0.0]\n"
           "        rotation:\n"
           "          - [1.0, 0.0, 0.0]\n"
           "          - [0.0, 1.0, 0.0]\n"
           "          - [0.0, 0.0, 1.0]\n"
        << model_specific
        << "      lookfrom: [0.0, 0.0, -3.0]\n"
           "      lookat: [0.0, 0.0, 0.0]\n"
           "      aspect_ratio: 1.3333333333333333\n"
           "      image_width: 64\n"
           "      max_depth: 8\n"
           "      vup: [0.0, 1.0, 0.0]\n"
           "      defocus_angle: 0.0\n"
           "      focus_dist: 3.0\n";
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

    const rt::PackedCamera pinhole_camera = rt::test::make_contract_test_pinhole_camera();
    Camera pinhole_seam;
    pinhole_seam.aspect_ratio =
        static_cast<double>(pinhole_camera.width) / static_cast<double>(pinhole_camera.height);
    pinhole_seam.image_width = pinhole_camera.width;
    pinhole_seam.samples_per_pixel = 1;
    pinhole_seam.defocus_angle = 0.0;
    pinhole_seam.focus_dist = 3.0;
    pinhole_seam.set_shared_camera_ray_config(make_camera_ray_config(pinhole_camera));
    const Eigen::Vector2d pinhole_pixel = rt::test::contract_pinhole_sample_pixel();
    const Ray pinhole_ray = pinhole_seam.debug_primary_ray(pinhole_pixel);
    expect_vec3_near(pinhole_ray.origin(), pinhole_camera.T_rc.translation(), 1e-12,
        "pinhole seam origin follows packed pose");
    expect_vec3_near(pinhole_ray.direction().normalized(),
        (pinhole_camera.T_rc.rotationMatrix()
            * rt::unproject_pinhole32(pinhole_camera.pinhole, pinhole_pixel))
            .normalized(),
        1e-12, "pinhole seam ray direction matches shared camera math");

    const rt::PackedCamera equi_camera = rt::test::make_contract_test_equi_camera();
    Camera equi_seam;
    equi_seam.aspect_ratio = static_cast<double>(equi_camera.width) / static_cast<double>(equi_camera.height);
    equi_seam.image_width = equi_camera.width;
    equi_seam.samples_per_pixel = 1;
    equi_seam.defocus_angle = 2.0;
    equi_seam.focus_dist = 3.0;
    equi_seam.set_shared_camera_ray_config(make_camera_ray_config(equi_camera));
    const Eigen::Vector2d equi_pixel = rt::test::contract_equi_sample_pixel();
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

    const fs::path root = fs::temp_directory_path() / "offline_shared_scene_model_switch";
    fs::remove_all(root);
    write_shared_scene_model_switch_scene(root / "pinhole" / "scene.yaml", "phase2_shared_pinhole", "pinhole32");
    write_shared_scene_model_switch_scene(root / "equi" / "scene.yaml", "phase2_shared_equi", "equi62_lut1d");

    rt::scene::global_scene_file_catalog().scan_directory(root);
    const cv::Mat shared_pinhole = rt::render_shared_scene("phase2_shared_pinhole", 1);
    const cv::Mat shared_equi = rt::render_shared_scene("phase2_shared_equi", 1);
    expect_true(!shared_pinhole.empty(), "shared-scene pinhole render should succeed");
    expect_true(!shared_equi.empty(), "shared-scene equi render should succeed");
    expect_true(shared_pinhole.cols == 64 && shared_pinhole.rows == 48,
        "shared-scene pinhole render keeps authored dimensions");
    expect_true(shared_equi.cols == 64 && shared_equi.rows == 48,
        "shared-scene equi render keeps authored dimensions");
    expect_true(cv::norm(shared_pinhole, shared_equi, cv::NORM_L1) > 0.0,
        "shared-scene preset path honors camera model changes");
    rt::scene::global_scene_file_catalog().scan_directory("assets/scenes");
    return 0;
}
