#include "core/core.h"

#include <fmt/core.h>
#include <Eigen/Eigen>
#include <cxxopts.hpp>
#include <opencv2/opencv.hpp>

#define checkCudaErrors(val) check_cuda((val), #val, __FILE__, __LINE__)

void check_cuda(cudaError_t result, char const* const func, const char* const file, int const line) {
    if (result) {
        fmt::print(stderr, "CUDA error = {} at {}:{} '{}'\n", static_cast<unsigned int>(result), file, line, func);
        // Make sure we call CUDA Device Reset before exiting
        cudaDeviceReset();
        exit(99);
    }
}

__global__ void render(float* buffer, int max_x, int max_y) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    int j = threadIdx.y + blockIdx.y * blockDim.y;
    if ((i >= max_x) || (j >= max_y)) {
        return;
    }
    int pixel_index = j * max_x * 3 + i * 3;
    buffer[pixel_index + 0] = float(i) / (max_x - 1);
    buffer[pixel_index + 1] = float(j) / (max_y - 1);
    buffer[pixel_index + 2] = 0.0;
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

    const int image_width = 256;
    const int image_height = 256;
    const int block_size_width = 8;
    const int block_size_height = 8;

    const int num_pixels = image_width * image_height;
    const size_t buffer_size = 3 * num_pixels * sizeof(float);

    float* buffer = nullptr;
    checkCudaErrors(cudaMallocManaged((void**)&buffer, buffer_size));

    dim3 blocks(image_width / block_size_width + 1, image_height / block_size_height + 1);
    dim3 threads(block_size_width, block_size_height);

    render<<<blocks, threads>>>(buffer, image_width, image_height);

    checkCudaErrors(cudaGetLastError());
    checkCudaErrors(cudaDeviceSynchronize());

    cv::Mat img(image_height, image_width, CV_8UC3);

    // Render

    for (int y = 0; y < image_height; y++) {
        for (int x = 0; x < image_width; x++) {
            const size_t pixel_index = y * 3 * image_width + x * 3;
            const double r = buffer[pixel_index + 0];
            const double g = buffer[pixel_index + 1];
            const double b = buffer[pixel_index + 2];

            const uint8_t ir = std::clamp(255.999 * r, 0.0, 255.0);
            const uint8_t ig = std::clamp(255.999 * g, 0.0, 255.0);
            const uint8_t ib = std::clamp(255.999 * b, 0.0, 255.0);

            img.at<cv::Vec3b>(y, x) = cv::Vec3b(ib, ig, ir);
        }
    }

    // cv::imshow("output image", img);
    // cv::waitKey();
    cv::imwrite(fmt::format("output.{}", output_image_format), img);

    checkCudaErrors(cudaFree(buffer));

    return 0;
}
