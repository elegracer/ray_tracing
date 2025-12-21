#include "common/ray.h"
#include "core/core.h"

#include <fmt/core.h>
#include <Eigen/Eigen>
#include <cxxopts.hpp>
#include <opencv2/opencv.hpp>

#include "common/color.h"

#define checkCudaErrors(val) check_cuda((val), #val, __FILE__, __LINE__)

void check_cuda(cudaError_t result, char const* const func, const char* const file,
    int const line) {
    if (result) {
        fmt::print(stderr, "CUDA error = {} at {}:{} '{}'\n", static_cast<unsigned int>(result),
            file, line, func);
        // Make sure we call CUDA Device Reset before exiting
        cudaDeviceReset();
        exit(99);
    }
}

__global__ void render(int* buffer, int max_x, int max_y, Eigen::Vector3d pixel00_loc,
    Eigen::Vector3d pixel_delta_u, Eigen::Vector3d pixel_delta_v, Eigen::Vector3d camera_center) {
    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;
    if ((x >= max_x) || (y >= max_y)) {
        return;
    }
    int pixel_index = y * max_x * 3 + x * 3;

    const Eigen::Vector3d pixel_center = pixel00_loc + (x * pixel_delta_u) + (y * pixel_delta_v);
    const Eigen::Vector3d ray_direction = pixel_center - camera_center;

    Ray ray(camera_center, ray_direction);

    const Eigen::Vector3i color = scale_normalized_color(ray_color(ray), 256);

    buffer[pixel_index + 0] = color.x();
    buffer[pixel_index + 1] = color.y();
    buffer[pixel_index + 2] = color.z();
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


    const int block_size_width = 8;
    const int block_size_height = 8;

    const int num_pixels = image_width * image_height;
    const size_t buffer_size = 3 * num_pixels * sizeof(int);

    int* buffer = nullptr;
    checkCudaErrors(cudaMallocManaged((void**)&buffer, buffer_size));

    dim3 blocks(image_width / block_size_width + 1, image_height / block_size_height + 1);
    dim3 threads(block_size_width, block_size_height);

    render<<<blocks, threads>>>(buffer, image_width, image_height, pixel00_loc, pixel_delta_u,
        pixel_delta_v, camera_center);

    checkCudaErrors(cudaGetLastError());
    checkCudaErrors(cudaDeviceSynchronize());

    cv::Mat img(image_height, image_width, CV_8UC3);

    // Render

    for (int y = 0; y < image_height; y++) {
        for (int x = 0; x < image_width; x++) {
            const size_t pixel_index = y * 3 * image_width + x * 3;
            const int r = buffer[pixel_index + 0];
            const int g = buffer[pixel_index + 1];
            const int b = buffer[pixel_index + 2];

            img.at<cv::Vec3b>(y, x) = cv::Vec3b(b, g, r);
        }
    }

    // cv::imshow("output image", img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), img);

    checkCudaErrors(cudaFree(buffer));

    return 0;
}
