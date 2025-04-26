#include "common/quad.h"
#include "common/sphere.h"
#include "core/version.h"

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <Eigen/Eigen>
#include <argparse/argparse.hpp>
#include <opencv2/opencv.hpp>

#include "common/common.h"
#include "common/camera.h"
#include "common/hittable_list.h"
#include "common/material.h"
#include "common/bvh.h"


void render_bouncing_spheres(const std::string& output_image_format) {

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
    cam.background = {0.70, 0.80, 1.00};

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
}

void render_checkered_spheres(const std::string& output_image_format) {

    // World
    HittableList world;

    auto checker =
        std::make_shared<CheckerTexture>(0.32, Vec3d {0.2, 0.3, 0.1}, Vec3d {0.9, 0.9, 0.9});

    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, -10.0, 0.0}, 10.0,
        pro::make_proxy_shared<Material, Lambertion>(checker)));
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 10.0, 0.0}, 10.0,
        pro::make_proxy_shared<Material, Lambertion>(checker)));

    auto bvhnode = pro::make_proxy_shared<Hittable, BVHNode>(world);

    pro::proxy<Hittable> world_as_hittable = bvhnode;

    // Camera
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = 500;
    cam.max_depth = 50;
    cam.background = {0.70, 0.80, 1.00};

    cam.vfov = 20.0;
    cam.lookfrom = {13.0, 2.0, 3.0};
    cam.lookat = {0.0, 0.0, 0.0};
    cam.vup = {0.0, 1.0, 0.0};

    cam.defocus_angle = 0.0;

    // Render
    cam.render(world_as_hittable);

    // cv::imshow("output image", cam.img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), cam.img);
}

void render_earth_sphere(const std::string& output_image_format) {
    // World
    HittableList world;

    auto earth_texture = pro::make_proxy_shared<Texture, ImageTexture>("earthmap.jpg");
    auto earth_surface = pro::make_proxy_shared<Material, Lambertion>(earth_texture);
    auto globe =
        pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 0.0, 0.0}, 2.0, earth_surface);

    world.add(globe);

    pro::proxy<Hittable> world_as_hittable = &world;

    // Camera
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = 500;
    cam.max_depth = 50;
    cam.background = {0.70, 0.80, 1.00};

    cam.vfov = 20.0;
    cam.lookfrom = {-3.0, 6.0, -10.0};
    cam.lookat = {0.0, 0.0, 0.0};
    cam.vup = {0.0, 1.0, 0.0};

    cam.defocus_angle = 0.0;

    cam.render(world_as_hittable);

    // cv::imshow("output image", cam.img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), cam.img);
}

void render_perlin_spheres(const std::string& output_image_format) {
    // World
    HittableList world;

    auto perlin_texture = pro::make_proxy_shared<Texture, NoiseTexture>(4.0);
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, -1000.0, 0.0}, 1000.0,
        pro::make_proxy_shared<Material, Lambertion>(perlin_texture)));
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 2.0, 0.0}, 2.0,
        pro::make_proxy_shared<Material, Lambertion>(perlin_texture)));

    pro::proxy<Hittable> world_as_hittable = &world;

    // Camera
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = 500;
    cam.max_depth = 50;
    cam.background = {0.70, 0.80, 1.00};

    cam.vfov = 20.0;
    cam.lookfrom = {13.0, 2.0, 3.0};
    cam.lookat = {0.0, 0.0, 0.0};
    cam.vup = {0.0, 1.0, 0.0};

    cam.defocus_angle = 0.0;

    cam.render(world_as_hittable);

    // cv::imshow("output image", cam.img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), cam.img);
}


void render_quads(const std::string& output_image_format) {
    // World
    HittableList world;

    // Materials
    auto left_red = pro::make_proxy_shared<Material, Lambertion>(Vec3d {1.0, 0.2, 0.2});
    auto back_green = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.2, 1.0, 0.2});
    auto right_blue = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.2, 0.2, 1.0});
    auto upper_orange = pro::make_proxy_shared<Material, Lambertion>(Vec3d {1.0, 0.5, 0.0});
    auto lower_teal = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.2, 0.8, 0.8});

    // Quads
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {-3.0, -2.0, 5.0},
        Vec3d {0.0, 0.0, -4.0}, Vec3d {0.0, 4.0, 0.0}, left_red));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {-2.0, -2.0, 0.0}, Vec3d {4.0, 0.0, 0.0},
        Vec3d {0.0, 4.0, 0.0}, back_green));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {3.0, -2.0, 1.0}, Vec3d {0.0, 0.0, 4.0},
        Vec3d {0.0, 4.0, 0.0}, right_blue));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {-2.0, 3.0, 1.0}, Vec3d {4.0, 0.0, 0.0},
        Vec3d {0.0, 0.0, 4.0}, upper_orange));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {-2.0, -3.0, 5.0}, Vec3d {4.0, 0.0, 0.0},
        Vec3d {0.0, 0.0, -4.0}, lower_teal));

    pro::proxy<Hittable> world_as_hittable = &world;

    // Camera
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = 500;
    cam.max_depth = 50;
    cam.background = {0.70, 0.80, 1.00};

    cam.vfov = 80.0;
    cam.lookfrom = {0.0, 0.0, 9.0};
    cam.lookat = {0.0, 0.0, 0.0};
    cam.vup = {0.0, 1.0, 0.0};

    cam.defocus_angle = 0.0;

    cam.render(world_as_hittable);

    // cv::imshow("output image", cam.img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), cam.img);
}

int main(int argc, const char* argv[]) {

    const std::string version_string = fmt::format("{}.{}.{}.{}", CORE_MAJOR_VERSION,
        CORE_MINOR_VERSION, CORE_PATCH_VERSION, CORE_TWEAK_VERSION);

    std::string output_image_format {};
    std::string scene_to_render {};

    argparse::ArgumentParser program("use_core", version_string);

    program.add_argument("--output_image_format")
        .help("Output Image Format, e.g. jpg, png, bmp etc.")
        .default_value("png")
        .store_into(output_image_format);

    program.add_argument("--scene")
        .help("Scene to render")
        .choices(                //
            "bouncing_spheres",  //
            "checkered_spheres", //
            "earth_sphere",      //
            "perlin_spheres",    //
            "quads")
        .default_value("quads")
        .store_into(scene_to_render);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        fmt::print(stderr, "{}\n\n", err.what());
        fmt::print(stderr, "{}\n", fmt::streamed(program));
        return EXIT_FAILURE;
    }

    fmt::print("output_image_format: {}\n", output_image_format);
    fmt::print("scene to render: {}\n", scene_to_render);

    if (scene_to_render == "bouncing_spheres") {
        render_bouncing_spheres(output_image_format);
    } else if (scene_to_render == "checkered_spheres") {
        render_checkered_spheres(output_image_format);
    } else if (scene_to_render == "earth_sphere") {
        render_earth_sphere(output_image_format);
    } else if (scene_to_render == "perlin_spheres") {
        render_perlin_spheres(output_image_format);
    } else if (scene_to_render == "quads") {
        render_quads(output_image_format);
    } else {
        fmt::print(stderr, "Invalid scene to render: '{}' !!!\n", scene_to_render);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
