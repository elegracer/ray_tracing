#include "common/camera.h"
#include "common/hittable_list.h"
#include "common/material.h"
#include "common/sphere.h"

#include <Eigen/Geometry>
#include <opencv2/opencv.hpp>

#include <cstdlib>
#include <iostream>

int main() {
    HittableList world;

    auto material = pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.8, 0.3, 0.3});
    world.add(
        pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 0.0, -1.0}, 0.5, material));

    pro::proxy<Hittable> world_as_hittable = &world;

    Camera cam;
    cam.aspect_ratio = 1.0;
    cam.image_width = 32;
    cam.samples_per_pixel = 1;
    cam.max_depth = 2;
    cam.background = {0.20, 0.30, 0.40};
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

    const cv::Scalar total = cv::sum(img);
    const double brightness = total[0] + total[1] + total[2];
    if (brightness <= 0.0) {
        std::cerr << "rendered image is uniformly black\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
