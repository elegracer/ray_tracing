#include "common/sphere.h"
#include "core/core.h"

#include <fmt/core.h>
#include <Eigen/Eigen>
#include <cxxopts.hpp>
#include <opencv2/opencv.hpp>

#include "common/defs.h"
#include "common/color.h"
#include "common/hittable_list.h"


inline Vec3i scale_normalized_color(const Vec3d& color_normalized, const int scale) {
    return (color_normalized * ((double)scale - 0.001))
        .cast<int>()
        .array()
        .max(0)
        .min(scale - 1)
        .matrix();
}


inline Vec3d ray_color(const Ray& ray, const pro::proxy<Hittable>& hittable) {
    HitRecord hit_rec;
    if (hittable->hit(ray, Interval {0, infinity}, hit_rec)) {
        return 0.5 * (hit_rec.normal + Vec3d::Ones());
    }

    const Vec3d unit_direction = ray.direction().normalized();
    const double a = 0.5 * (unit_direction.y() + 1.0);
    return (1.0 - a) * Vec3d {1.0, 1.0, 1.0} + a * Vec3d {0.5, 0.7, 1.0};
}


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

    // Image

    const double aspect_ratio = 16.0 / 9.0;
    const int image_width = 1280;

    // Calculate the image height, and ensure that it's at least 1
    const int image_height = std::max(int(image_width / aspect_ratio), 1);

    // World
    HittableList world;
    world.add(pro::make_proxy<Hittable, Sphere>(Vec3d {0.0, 0.0, -1.0}, 0.5));
    world.add(pro::make_proxy<Hittable, Sphere>(Vec3d {0.0, -100.5, -1.0}, 100.0));

    pro::proxy<Hittable> world_as_hittable = &world;

    // Camera
    const double focal_length = 1.0;
    const double viewport_height = 2.0;
    const double viewport_width = viewport_height * (double(image_width) / image_height);
    const Vec3d camera_center {0.0, 0.0, 0.0};

    // Calculate the vectors across the horizontal and down the vertical viewport edges
    const Vec3d viewport_u {viewport_width, 0.0, 0.0};
    const Vec3d viewport_v {0.0, -viewport_height, 0.0};

    // Calculate the horizontal and vertical delta vectors from pixel to pixel
    const Vec3d pixel_delta_u = viewport_u / image_width;
    const Vec3d pixel_delta_v = viewport_v / image_height;

    // Calculate the location of the upper left pixel
    const Vec3d viewport_upper_left =
        camera_center - Vec3d {0.0, 0.0, focal_length} - viewport_u / 2 - viewport_v / 2;
    const Vec3d pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // Render
    cv::Mat img(image_height, image_width, CV_8UC3);

    for (int y = 0; y < image_height; y++) {
        for (int x = 0; x < image_width; x++) {
            const Vec3d pixel_center = pixel00_loc + (x * pixel_delta_u) + (y * pixel_delta_v);
            const Vec3d ray_direction = pixel_center - camera_center;

            Ray ray(camera_center, ray_direction);

            const Vec3i color = scale_normalized_color(ray_color(ray, world_as_hittable), 256);

            img.at<cv::Vec3b>(y, x) = cv::Vec3b(color.z(), color.y(), color.x());
        }
    }

    // cv::imshow("output image", img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), img);

    return 0;
}
