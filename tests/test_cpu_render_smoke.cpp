#include "common/camera.h"
#include "common/hittable_list.h"
#include "common/material.h"
#include "common/quad.h"
#include "common/sphere.h"

#include <Eigen/Geometry>
#include <opencv2/opencv.hpp>

#include <cstdlib>
#include <iostream>

int main() {
    HittableList world;

    auto light = pro::make_proxy_shared<Material, DiffuseLight>(Vec3d {8.0, 8.0, 8.0});
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {-2.0, -2.0, -1.0},
        Vec3d {4.0, 0.0, 0.0}, Vec3d {0.0, 4.0, 0.0}, light));

    pro::proxy<Hittable> world_as_hittable = &world;

    Camera cam;
    cam.aspect_ratio = 1.0;
    cam.image_width = 32;
    cam.samples_per_pixel = 4;
    cam.max_depth = 1;
    cam.background = {0.0, 0.0, 0.0};
    cam.vfov = 90.0;
    cam.lookfrom = {0.0, 0.0, 0.0};
    cam.lookat = {0.0, 0.0, -1.0};
    cam.vup = {0.0, 1.0, 0.0};
    cam.defocus_angle = 0.0;

    cam.render(world_as_hittable);

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
    if (non_black_pixels < 512) {
        std::cerr << "too few lit pixels: " << non_black_pixels << "\n";
        return EXIT_FAILURE;
    }

    const cv::Rect center_rect {img.cols / 2 - 3, img.rows / 2 - 3, 6, 6};
    const cv::Mat center = gray(center_rect);
    const int center_lit_pixels = cv::countNonZero(center);
    if (center_lit_pixels < 30) {
        std::cerr << "center region is too dim: " << center_lit_pixels << " lit pixels\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
