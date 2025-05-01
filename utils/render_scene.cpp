#include "core/version.h"

#include "common/common.h"
#include "common/hittable.h"
#include "common/hittable_list.h"
#include "common/constant_medium.h"
#include "common/quad.h"
#include "common/sphere.h"

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


constexpr uint64_t hash(std::string_view sv) {
    uint64_t hash = 0;
    for (char c : sv) {
        hash = (hash * 131) + c;
    }
    return hash;
}

constexpr uint64_t operator""_hash(const char* str, size_t len) {
    return hash({str, len});
}


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


void render_simple_light(const std::string& output_image_format) {
    // World
    HittableList world;

    auto perlin_texture = pro::make_proxy_shared<Texture, NoiseTexture>(4.0);
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, -1000.0, 0.0}, 1000.0,
        pro::make_proxy_shared<Material, Lambertion>(perlin_texture)));
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 2.0, 0.0}, 2.0,
        pro::make_proxy_shared<Material, Lambertion>(perlin_texture)));

    auto diffuse_light = pro::make_proxy_shared<Material, DiffuseLight>(Vec3d {4.0, 4.0, 4.0});
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 7.0, 0.0}, 2.0, diffuse_light));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {3.0, 1.0, -2.0}, Vec3d {2.0, 0.0, 0.0},
        Vec3d {0.0, 2.0, 0.0}, diffuse_light));

    pro::proxy<Hittable> world_as_hittable = &world;

    // Camera
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = 500;
    cam.max_depth = 50;
    cam.background = {0.0, 0.0, 0.0};

    cam.vfov = 20.0;
    cam.lookfrom = {26.0, 3.0, 6.0};
    cam.lookat = {0.0, 2.0, 0.0};
    cam.vup = {0.0, 1.0, 0.0};

    cam.defocus_angle = 0.0;

    cam.render(world_as_hittable);

    // cv::imshow("output image", cam.img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), cam.img);
}


void render_cornell_box(const std::string& output_image_format) {
    // World
    HittableList world;

    auto red = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.65, 0.05, 0.05});
    auto white = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.73, 0.73, 0.73});
    auto green = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.12, 0.45, 0.15});
    auto light = pro::make_proxy_shared<Material, DiffuseLight>(Vec3d {15.0, 15.0, 15.0});

    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {555.0, 0.0, 0.0},
        Vec3d {0.0, 555.0, 0.0}, Vec3d {0.0, 0.0, 555.0}, green));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {0.0, 0.0, 0.0}, Vec3d {0.0, 555.0, 0.0},
        Vec3d {0.0, 0.0, 555.0}, red));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {343.0, 554.0, 332.0},
        Vec3d {-130.0, 0.0, 0.0}, Vec3d {0.0, 0.0, -105.0}, light));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {0.0, 0.0, 0.0}, Vec3d {555.0, 0.0, 0.0},
        Vec3d {0.0, 0.0, 555.0}, white));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {555.0, 555.0, 555.0},
        Vec3d {-555.0, 0.0, 0.0}, Vec3d {0.0, 0.0, -555.0}, white));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {0.0, 0.0, 555.0},
        Vec3d {555.0, 0.0, 0.0}, Vec3d {0.0, 555.0, 0.0}, white));

    auto aluminum = pro::make_proxy_shared<Material, Metal>(Vec3d {0.8, 0.85, 0.88}, 0.0);
    auto box1 = box(Vec3d {0.0, 0.0, 0.0}, Vec3d {165.0, 330.0, 165.0}, aluminum);
    box1 = pro::make_proxy_shared<Hittable, RotateY>(box1, 15.0);
    box1 = pro::make_proxy_shared<Hittable, Translate>(box1, Vec3d {265.0, 0.0, 295.0});

    auto box2 = box(Vec3d {0.0, 0.0, 0.0}, Vec3d {165.0, 165.0, 165.0}, white);
    box2 = pro::make_proxy_shared<Hittable, RotateY>(box2, -18.0);
    box2 = pro::make_proxy_shared<Hittable, Translate>(box2, Vec3d {130.0, 0.0, 65.0});

    world.add(box1);
    world.add(box2);

    // Light sources
    auto empty_material = pro::make_proxy_shared<Material, EmptyMaterial>();
    auto lights = pro::make_proxy_shared<Hittable, Quad>(Vec3d {343.0, 554.0, 332.0},
        Vec3d {-130.0, 0.0, 0.0}, Vec3d {0.0, 0.0, -105.0}, empty_material);

    pro::proxy<Hittable> world_as_hittable = &world;

    // Camera
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = 1000;
    cam.max_depth = 50;
    cam.background = {0.0, 0.0, 0.0};

    cam.vfov = 40.0;
    cam.lookfrom = {278.0, 278.0, -800.0};
    cam.lookat = {278.0, 278.0, 0.0};
    cam.vup = {0.0, 1.0, 0.0};

    cam.defocus_angle = 0.0;

    cam.render(world_as_hittable, lights);

    // cv::imshow("output image", cam.img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), cam.img);
}

void render_cornell_smoke(const std::string& output_image_format) {
    // World
    HittableList world;

    auto red = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.65, 0.05, 0.05});
    auto white = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.73, 0.73, 0.73});
    auto green = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.12, 0.45, 0.15});
    auto light = pro::make_proxy_shared<Material, DiffuseLight>(Vec3d {15.0, 15.0, 15.0});

    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {555.0, 0.0, 0.0},
        Vec3d {0.0, 555.0, 0.0}, Vec3d {0.0, 0.0, 555.0}, green));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {0.0, 0.0, 0.0}, Vec3d {0.0, 555.0, 0.0},
        Vec3d {0.0, 0.0, 555.0}, red));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {343.0, 554.0, 332.0},
        Vec3d {-130.0, 0.0, 0.0}, Vec3d {0.0, 0.0, -105.0}, light));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {0.0, 0.0, 0.0}, Vec3d {555.0, 0.0, 0.0},
        Vec3d {0.0, 0.0, 555.0}, white));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {555.0, 555.0, 555.0},
        Vec3d {-555.0, 0.0, 0.0}, Vec3d {0.0, 0.0, -555.0}, white));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {0.0, 0.0, 555.0},
        Vec3d {555.0, 0.0, 0.0}, Vec3d {0.0, 555.0, 0.0}, white));

    auto box1 = box(Vec3d {0.0, 0.0, 0.0}, Vec3d {165.0, 330.0, 165.0}, white);
    box1 = pro::make_proxy_shared<Hittable, RotateY>(box1, 15.0);
    box1 = pro::make_proxy_shared<Hittable, Translate>(box1, Vec3d {265.0, 0.0, 295.0});

    auto box2 = box(Vec3d {0.0, 0.0, 0.0}, Vec3d {165.0, 165.0, 165.0}, white);
    box2 = pro::make_proxy_shared<Hittable, RotateY>(box2, -18.0);
    box2 = pro::make_proxy_shared<Hittable, Translate>(box2, Vec3d {130.0, 0.0, 65.0});

    world.add(pro::make_proxy_shared<Hittable, ConstantMedium>(box1, 0.01, Vec3d {0.0, 0.0, 0.0}));
    world.add(pro::make_proxy_shared<Hittable, ConstantMedium>(box2, 0.01, Vec3d {1.0, 1.0, 1.0}));

    pro::proxy<Hittable> world_as_hittable = &world;

    // Camera
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = 500;
    cam.max_depth = 50;
    cam.background = {0.0, 0.0, 0.0};

    cam.vfov = 40.0;
    cam.lookfrom = {278.0, 278.0, -800.0};
    cam.lookat = {278.0, 278.0, 0.0};
    cam.vup = {0.0, 1.0, 0.0};

    cam.defocus_angle = 0.0;

    cam.render(world_as_hittable);

    // cv::imshow("output image", cam.img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), cam.img);
}

void render_rttnw_final_scene(const std::string& output_image_format,
    const int samples_per_pixel = 500) {
    // World
    HittableList world;

    HittableList boxes1;
    auto ground = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.48, 0.83, 0.53});
    constexpr int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; ++i) {
        for (int j = 0; j < boxes_per_side; ++j) {
            constexpr double w = 100.0;
            const double x0 = -1000.0 + i * w;
            const double y0 = 0.0;
            const double z0 = -1000.0 + j * w;
            const double x1 = x0 + w;
            const double y1 = random_double(1.0, 101.0);
            const double z1 = z0 + w;

            boxes1.add(box(Vec3d {x0, y0, z0}, Vec3d {x1, y1, z1}, ground));
        }
    }
    world.add(pro::make_proxy_shared<Hittable, BVHNode>(boxes1));

    auto light = pro::make_proxy_shared<Material, DiffuseLight>(Vec3d {7.0, 7.0, 7.0});
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {123.0, 554.0, 147.0},
        Vec3d {300.0, 0.0, 0.0}, Vec3d {0.0, 0.0, 265.0}, light));

    const Vec3d center1 {400.0, 400.0, 200.0};
    const Vec3d center2 = center1 + Vec3d {30.0, 0.0, 0.0};
    auto sphere_material = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.7, 0.3, 0.1});
    world.add(pro::make_proxy_shared<Hittable, Sphere>(center1, center2, 50.0, sphere_material));

    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {260.0, 150.0, 45.0}, 50.0,
        pro::make_proxy_shared<Material, Dielectric>(1.5)));
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 150.0, 145.0}, 50.0,
        pro::make_proxy_shared<Material, Metal>(Vec3d {0.8, 0.8, 0.9}, 1.0)));

    auto boundary = pro::make_proxy_shared<Hittable, Sphere>(Vec3d {360.0, 150.0, 145.0}, 70.0,
        pro::make_proxy_shared<Material, Dielectric>(1.5));
    world.add(boundary);
    world.add(
        pro::make_proxy_shared<Hittable, ConstantMedium>(boundary, 0.2, Vec3d {0.2, 0.4, 0.9}));
    boundary = pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 0.0, 0.0}, 5000.0,
        pro::make_proxy_shared<Material, Dielectric>(1.5));
    world.add(
        pro::make_proxy_shared<Hittable, ConstantMedium>(boundary, 0.0001, Vec3d {1.0, 1.0, 1.0}));

    auto emissive_mat = pro::make_proxy_shared<Material, Lambertion>(
        pro::make_proxy_shared<Texture, ImageTexture>("earthmap.jpg"));
    world.add(
        pro::make_proxy_shared<Hittable, Sphere>(Vec3d {400.0, 200.0, 400.0}, 100.0, emissive_mat));
    auto perlin_texture = pro::make_proxy_shared<Texture, NoiseTexture>(0.2);
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {220.0, 280.0, 300.0}, 80.0,
        pro::make_proxy_shared<Material, Lambertion>(perlin_texture)));

    HittableList boxes2;
    auto white = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.73, 0.73, 0.73});
    constexpr int ns = 1000;
    for (int j = 0; j < ns; ++j) {
        const Vec3d random_vec = 0.5 * 165.0 * (Vec3d::Random().array() + 1.0);
        boxes2.add(pro::make_proxy_shared<Hittable, Sphere>(random_vec, 10.0, white));
    }

    world.add(                                                     //
        pro::make_proxy_shared<Hittable, Translate>(               //
            pro::make_proxy_shared<Hittable, RotateY>(             //
                pro::make_proxy_shared<Hittable, BVHNode>(boxes2), //
                15.0),
            Vec3d {-100.0, 270.0, 395.0}));

    pro::proxy<Hittable> world_as_hittable = &world;

    // Camera
    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = 50;
    cam.background = {0.0, 0.0, 0.0};

    cam.vfov = 40.0;
    cam.lookfrom = {478.0, 278.0, -600.0};
    cam.lookat = {278.0, 278.0, 0.0};
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
            "quads",             //
            "simple_light",      //
            "cornell_box",       //
            "cornell_smoke",     //
            "rttnw_final_scene", //
            "rttnw_final_scene_extreme")
        .default_value("cornell_box")
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

    switch (hash(scene_to_render)) {
        case "bouncing_spheres"_hash: {
            render_bouncing_spheres(output_image_format);
        } break;
        case "checkered_spheres"_hash: {
            render_checkered_spheres(output_image_format);
        } break;
        case "earth_sphere"_hash: {
            render_earth_sphere(output_image_format);
        } break;
        case "perlin_spheres"_hash: {
            render_perlin_spheres(output_image_format);
        } break;
        case "quads"_hash: {
            render_quads(output_image_format);
        } break;
        case "simple_light"_hash: {
            render_simple_light(output_image_format);
        } break;
        case "cornell_box"_hash: {
            render_cornell_box(output_image_format);
        } break;
        case "cornell_smoke"_hash: {
            render_cornell_smoke(output_image_format);
        } break;
        case "rttnw_final_scene"_hash: {
            render_rttnw_final_scene(output_image_format);
        } break;
        case "rttnw_final_scene_extreme"_hash: {
            render_rttnw_final_scene(output_image_format, 10000);
        } break;
        default: {
            fmt::print(stderr, "Invalid scene to render: '{}' !!!\n", scene_to_render);
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
