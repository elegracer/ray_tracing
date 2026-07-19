#include "realtime/display_transfer.h"
#include "realtime/viewer/cuda_gl_presenter.h"
#include "test_support.h"

#include <GLFW/glfw3.h>
#include <cuda_runtime.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

void check_cuda(cudaError_t error, const char* expression) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string(expression) + ": " + cudaGetErrorString(error));
    }
}

} // namespace

int main() {
    rt::viewer::configure_cuda_gl_interop_environment();
    if (!glfwInit()) {
        return 77;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(32, 32, "cuda_gl_presenter_test", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 77;
    }
    glfwMakeContextCurrent(window);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    const std::array<float4, 4> source {{
        make_float4(0.0f, 0.25f, 1.0f, 0.0f),
        make_float4(4.0f, -1.0f, 0.01f, 0.0f),
        make_float4(0.04f, 0.16f, 0.36f, 0.0f),
        make_float4(0.64f, 0.81f, 0.99f, 0.0f),
    }};
    float4* device_source = nullptr;
    cudaStream_t stream = nullptr;
    check_cuda(cudaMalloc(reinterpret_cast<void**>(&device_source), sizeof(source)), "cudaMalloc");
    check_cuda(cudaStreamCreate(&stream), "cudaStreamCreate");
    check_cuda(cudaMemcpyAsync(device_source, source.data(), sizeof(source), cudaMemcpyHostToDevice,
                   stream),
        "cudaMemcpyAsync");

    {
        rt::viewer::CudaGlTexturePresenter presenter(texture, 2, 2);
        expect_true(presenter.present(rt::DeviceRadianceFrameView {
                        .beauty_rgba = nullptr,
                        .width = 2,
                        .height = 2,
                        .stream = stream,
                    }) == rt::viewer::CudaGlPresentStatus::invalid_device_view,
            "reject null device view");
        expect_true(presenter.present(rt::DeviceRadianceFrameView {
                        .beauty_rgba = device_source,
                        .width = 3,
                        .height = 2,
                        .stream = stream,
                    }) == rt::viewer::CudaGlPresentStatus::resolution_mismatch,
            "reject resolution mismatch");
        expect_true(presenter.present(rt::DeviceRadianceFrameView {
                        .beauty_rgba = device_source,
                        .width = 2,
                        .height = 2,
                        .stream = stream,
                    }) == rt::viewer::CudaGlPresentStatus::ok,
            "present device beauty through CUDA/OpenGL interop");

        glFinish();
        std::array<std::uint8_t, 16> pixels {};
        glBindTexture(GL_TEXTURE_2D, texture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        for (std::size_t pixel = 0; pixel < source.size(); ++pixel) {
            expect_true(pixels[pixel * 4 + 0] == rt::linear_to_display_u8(source[pixel].x),
                "red display transfer");
            expect_true(pixels[pixel * 4 + 1] == rt::linear_to_display_u8(source[pixel].y),
                "green display transfer");
            expect_true(pixels[pixel * 4 + 2] == rt::linear_to_display_u8(source[pixel].z),
                "blue display transfer");
            expect_true(pixels[pixel * 4 + 3] == 255, "opaque alpha");
        }
    }

    check_cuda(cudaStreamDestroy(stream), "cudaStreamDestroy");
    check_cuda(cudaFree(device_source), "cudaFree");
    glDeleteTextures(1, &texture);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
