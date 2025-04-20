#include "common/sphere.h"
#include "core/core.h"

#include <fmt/core.h>
#include <Eigen/Eigen>
#include <cxxopts.hpp>
#include <opencv2/opencv.hpp>

#include "common/common.h"
#include "common/camera.h"
#include "common/hittable_list.h"
#include "common/material.h"
#include "common/bvh.h"

int main(int argc, const char* argv[]) {

    std::string output_image_format = "png";

    cxxopts::Options options("use_core", core::get_project_description());
    options                           //
        .set_tab_expansion()          //
        .allow_unrecognised_options() //
        .add_options()                //
        ("output_image_format", "Output Image Format, e.g. jpg, png, bmp etc.",
            cxxopts::value<std::string>(output_image_format));

    const auto result = options.parse(argc, argv);

    if (result.count("output_image_format")) {
        fmt::print("parsed output_image_format: {}\n", output_image_format);
    } else {
        fmt::print("use default output_image_format: {}\n", output_image_format);
    }

    // World
    HittableList world;

    auto ground_material = std::make_shared<Lambertion>(Vec3d {0.5, 0.5, 0.5});
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, -1000.0, 0.0}, 1000.0,
        ground_material));

    for (int a = -11; a < 11; ++a) {
        for (int b = -11; b < 11; ++b) {
            const double choose_mat = random_double();
            const Vec3d center {a + 0.5 * random_double(), 0.2, b + 0.5 * random_double()};

            if ((center - Vec3d {4.0, 0.2, 0.0}).norm() > 0.9) {
                if (choose_mat < 0.8) {
                    // diffuse
                    const Vec3d albedo = random_vec3d().array() * random_vec3d().array();
                    auto sphere_material = std::make_shared<Lambertion>(albedo);
                    const Vec3d center2 = center + Vec3d {0.0, random_double(0.0, 0.5), 0.0};
                    world.add(pro::make_proxy_shared<Hittable, Sphere>(center, center2, 0.2,
                        sphere_material));
                } else if (choose_mat < 0.95) {
                    // metal
                    const Vec3d albedo = random_vec3d(0.5, 1.0);
                    const double fuzz = random_double(0.0, 0.5);
                    auto sphere_material = std::make_shared<Metal>(albedo, fuzz);
                    world.add(
                        pro::make_proxy_shared<Hittable, Sphere>(center, 0.2, sphere_material));
                } else {
                    // glass
                    auto sphere_material = std::make_shared<Dielectric>(1.5);
                    world.add(
                        pro::make_proxy_shared<Hittable, Sphere>(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material_1 = std::make_shared<Dielectric>(1.50);
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 1.0, 0.0}, 1.0, material_1));

    auto material_2 = std::make_shared<Lambertion>(Vec3d {0.4, 0.2, 0.1});
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {-4.0, 1.0, 0.0}, 1.0, material_2));

    auto material_3 = std::make_shared<Metal>(Vec3d {0.7, 0.6, 0.5}, 0.0);
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {4.0, 1.0, 0.0}, 1.0, material_3));

    auto bvhnode = pro::make_proxy_shared<Hittable, BVHNode>(world);

    pro::proxy<Hittable> world_as_hittable = bvhnode;

    // Camera
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = 500;
    cam.max_depth = 50;

    cam.vfov = 20.0;
    cam.lookfrom = {13.0, 2.0, 3.0};
    cam.lookat = {0.0, 0.0, 0.0};
    cam.vup = {0.0, 1.0, 0.0};

    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    // Render
    cam.render(world_as_hittable);

    // cv::imshow("output image", cam.img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), cam.img);

    return 0;
}
