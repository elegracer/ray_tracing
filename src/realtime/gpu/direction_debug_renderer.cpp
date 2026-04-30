#include "realtime/gpu/direction_debug_renderer.h"

#include "realtime/gpu/radiance_launch_setup.h"

#include <stdexcept>
#include <string>

namespace rt {

void launch_direction_debug_kernel(
    const DeviceActiveCamera& camera, std::uint8_t* rgba, int width, int height, cudaStream_t stream);

namespace {

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA runtime failure at ") + expr + ": "
            + cudaGetErrorString(error));
    }
}

#define RT_CUDA_CHECK(expr) throw_cuda_error((expr), #expr)

class DeviceRgbaBuffer {
   public:
    explicit DeviceRgbaBuffer(std::size_t byte_count) {
        RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&rgba_), byte_count));
    }

    ~DeviceRgbaBuffer() {
        if (rgba_ != nullptr) {
            cudaFree(rgba_);
        }
    }

    DeviceRgbaBuffer(const DeviceRgbaBuffer&) = delete;
    DeviceRgbaBuffer& operator=(const DeviceRgbaBuffer&) = delete;
    DeviceRgbaBuffer(DeviceRgbaBuffer&&) = delete;
    DeviceRgbaBuffer& operator=(DeviceRgbaBuffer&&) = delete;

    std::uint8_t* get() const {
        return rgba_;
    }

   private:
    std::uint8_t* rgba_ = nullptr;
};

}  // namespace

DirectionDebugFrame render_direction_debug_frame(const PackedCamera& camera, cudaStream_t stream) {
    DirectionDebugFrame frame {};
    frame.width = camera.width;
    frame.height = camera.height;
    frame.rgba.resize(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4U, 0);

    const std::size_t byte_count = frame.rgba.size() * sizeof(std::uint8_t);
    DeviceRgbaBuffer device_rgba(byte_count);
    launch_direction_debug_kernel(
        make_device_active_camera(camera), device_rgba.get(), frame.width, frame.height, stream);
    RT_CUDA_CHECK(cudaMemcpyAsync(frame.rgba.data(), device_rgba.get(), byte_count, cudaMemcpyDeviceToHost, stream));
    RT_CUDA_CHECK(cudaStreamSynchronize(stream));

    return frame;
}

}  // namespace rt
