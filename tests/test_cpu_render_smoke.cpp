#include "common/camera.h"
#include "common/hittable_list.h"
#include "common/material.h"
#include "common/quad.h"
#include "common/sphere.h"

#include <Eigen/Geometry>
#include <opencv2/opencv.hpp>
#include <tbb/global_control.h>

#include <cstdlib>
#include <algorithm>
#include <iostream>

int main() {
    tbb::global_control render_threads(
        tbb::global_control::max_allowed_parallelism, 1);

    HittableList world;
    HittableList lights;

    auto sphere_material = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.75, 0.25, 0.2});
    auto light_material = pro::make_proxy_shared<Material, DiffuseLight>(Vec3d {10.0, 10.0, 10.0});
    auto light_proxy = pro::make_proxy_shared<Hittable, Quad>(Vec3d {-0.75, 1.25, -1.5},
        Vec3d {1.5, 0.0, 0.0}, Vec3d {0.0, 0.0, 1.5}, light_material);

    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 0.0, -1.0}, 0.5,
        sphere_material));
    world.add(light_proxy);
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

    const cv::Mat& img = cam.img;
    if (img.empty() || img.data == nullptr) {
        std::cerr << "rendered image is empty\n";
        return EXIT_FAILURE;
    }

    if (img.rows != 32 || img.cols != 32) {
        std::cerr << "unexpected image dimensions: " << img.cols << "x" << img.rows << "\n";
        return EXIT_FAILURE;
    }

    if (img.type() != CV_8UC3) {
        std::cerr << "unexpected image type: " << img.type() << "\n";
        return EXIT_FAILURE;
    }

    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

    const int non_black_pixels = cv::countNonZero(gray);
    if (non_black_pixels < 96 || non_black_pixels > 420) {
        std::cerr << "lit pixel count out of expected range: " << non_black_pixels << "\n";
        return EXIT_FAILURE;
    }

    const cv::Rect center_rect {img.cols / 2 - 3, img.rows / 2 - 3, 6, 6};
    const cv::Rect corner_tl {0, 0, 6, 6};
    const cv::Rect corner_tr {img.cols - 6, 0, 6, 6};
    const cv::Rect corner_bl {0, img.rows - 6, 6, 6};
    const cv::Rect corner_br {img.cols - 6, img.rows - 6, 6, 6};
    const double center_mean = cv::mean(gray(center_rect))[0];
    const double corner_mean = std::max({cv::mean(gray(corner_tl))[0], cv::mean(gray(corner_tr))[0],
        cv::mean(gray(corner_bl))[0], cv::mean(gray(corner_br))[0]});

    if (center_mean < 18.0) {
        std::cerr << "center region is too dim: " << center_mean << "\n";
        return EXIT_FAILURE;
    }

    if (corner_mean > 6.0) {
        std::cerr << "corner region is too bright: " << corner_mean << "\n";
        return EXIT_FAILURE;
    }

    if (center_mean < (corner_mean + 10.0)) {
        std::cerr << "center/corner contrast is too small: " << center_mean << " vs "
                  << corner_mean << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
