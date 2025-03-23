#include "core/core.h"

#include <fmt/core.h>
#include <Eigen/Eigen>
#include <cxxopts.hpp>
#include <opencv2/opencv.hpp>

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

    const int image_width = 256;
    const int image_height = 256;

    cv::Mat img(image_height, image_width, CV_8UC3);

    // Render

    for (int y = 0; y < image_height; y++) {
        for (int x = 0; x < image_width; x++) {
            const double r = double(x) / (image_width - 1);
            const double g = double(y) / (image_height - 1);
            const double b = 0.0;

            const uint8_t ir = std::clamp(255.999 * r, 0.0, 255.0);
            const uint8_t ig = std::clamp(255.999 * g, 0.0, 255.0);
            const uint8_t ib = std::clamp(255.999 * b, 0.0, 255.0);

            img.at<cv::Vec3b>(y, x) = cv::Vec3b(ib, ig, ir);
        }
    }

    // cv::imshow("output image", img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), img);

    return 0;
}
