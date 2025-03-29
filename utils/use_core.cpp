#include "common/ray.h"
#include "core/core.h"

#include <fmt/core.h>
#include <Eigen/Eigen>
#include <cxxopts.hpp>
#include <opencv2/opencv.hpp>

#include "common/color.h"

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
    const int image_width = 400;

    // Calculate the image height, and ensure that it's at least 1
    const int image_height = std::max(int(image_width / aspect_ratio), 1);

    // Camera
    const double focal_length = 1.0;
    const double viewport_height = 2.0;
    const double viewport_width = viewport_height * (double(image_width) / image_height);
    const Eigen::Vector3d camera_center {0.0, 0.0, 0.0};

    // Calculate the vectors across the horizontal and down the vertical viewport edges
    const Eigen::Vector3d viewport_u {viewport_width, 0.0, 0.0};
    const Eigen::Vector3d viewport_v {0.0, -viewport_height, 0.0};

    // Calculate the horizontal and vertical delta vectors from pixel to pixel
    const Eigen::Vector3d pixel_delta_u = viewport_u / image_width;
    const Eigen::Vector3d pixel_delta_v = viewport_v / image_height;

    // Calculate the location of the upper left pixel
    const Eigen::Vector3d viewport_upper_left =
        camera_center - Eigen::Vector3d {0.0, 0.0, focal_length} - viewport_u / 2 - viewport_v / 2;
    const Eigen::Vector3d pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // Render
    cv::Mat img(image_height, image_width, CV_8UC3);

    for (int y = 0; y < image_height; y++) {
        for (int x = 0; x < image_width; x++) {
            const Eigen::Vector3d pixel_center =
                pixel00_loc + (x * pixel_delta_u) + (y * pixel_delta_v);
            const Eigen::Vector3d ray_direction = pixel_center - camera_center;

            Ray ray(camera_center, ray_direction);

            const Eigen::Vector3i color = ray_color(ray);

            img.at<cv::Vec3b>(y, x) = cv::Vec3b(color.z(), color.y(), color.x());
        }
    }

    // cv::imshow("output image", img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), img);

    return 0;
}
