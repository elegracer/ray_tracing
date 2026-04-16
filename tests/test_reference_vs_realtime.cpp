#include "common/camera.h"
#include "common/hittable.h"
#include "common/hittable_list.h"
#include "common/material.h"
#include "common/quad.h"
#include "common/sphere.h"
#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

#include <opencv2/opencv.hpp>
#include <tbb/global_control.h>

namespace {

double compute_cpu_reference_mean_luminance() {
    tbb::global_control render_threads(
        tbb::global_control::max_allowed_parallelism, 1);

    HittableList world;
    HittableList lights;

    auto diffuse = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.75, 0.25, 0.2});
    auto light = pro::make_proxy_shared<Material, DiffuseLight>(Vec3d {10.0, 10.0, 10.0});

    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 0.0, -1.0}, 0.5, diffuse));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {-0.75, 1.25, -1.5},
        Vec3d {1.5, 0.0, 0.0}, Vec3d {0.0, 0.0, 1.5}, light));
    lights.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {-0.75, 1.25, -1.5},
        Vec3d {1.5, 0.0, 0.0}, Vec3d {0.0, 0.0, 1.5},
        pro::make_proxy_shared<Material, EmptyMaterial>()));

    pro::proxy<Hittable> world_as_hittable = &world;
    pro::proxy<Hittable> lights_as_hittable = &lights;

    Camera cam;
    cam.aspect_ratio = 1.0;
    cam.image_width = 32;
    cam.samples_per_pixel = 16;
    cam.max_depth = 4;
    cam.background = {0.0, 0.0, 0.0};
    cam.vfov = 90.0;
    cam.lookfrom = {0.0, 0.0, 0.0};
    cam.lookat = {0.0, 0.0, -1.0};
    cam.vup = {0.0, 1.0, 0.0};
    cam.defocus_angle = 0.0;

    cam.render(world_as_hittable, lights_as_hittable);

    double sum = 0.0;
    for (int y = 0; y < cam.img.rows; ++y) {
        for (int x = 0; x < cam.img.cols; ++x) {
            const cv::Vec3b pixel = cam.img.at<cv::Vec3b>(y, x);
            sum += (pixel[0] + pixel[1] + pixel[2]) / (3.0 * 255.0);
        }
    }
    return sum / static_cast<double>(cam.img.rows * cam.img.cols);
}

}  // namespace

int main() {
    rt::SceneDescription scene;
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.75, 0.25, 0.2}});
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {10.0, 10.0, 10.0}});
    scene.add_sphere(rt::SpherePrimitive {diffuse, Eigen::Vector3d {0.0, 0.0, -1.0}, 0.5, false});
    scene.add_quad(rt::QuadPrimitive {
        light,
        Eigen::Vector3d {-0.75, 1.25, -1.5},
        Eigen::Vector3d {1.5, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 1.5},
        false,
    });

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {150.0, 150.0, 16.0, 16.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 32, 32);

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 16;

    rt::OptixRenderer renderer;
    const rt::RadianceFrame gpu = renderer.render_radiance(scene.pack(), rig.pack(), profile, 0);
    expect_true(gpu.average_luminance > 0.01, "gpu frame is lit");
    const double cpu_mean_luminance = compute_cpu_reference_mean_luminance();
    expect_near(gpu.average_luminance, cpu_mean_luminance, 0.05, "mean luminance agreement");
    return 0;
}
