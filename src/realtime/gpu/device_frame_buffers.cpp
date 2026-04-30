#include "realtime/gpu/device_frame_buffers.h"

#include <stdexcept>
#include <string>

namespace rt {
namespace {

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA runtime failure at ") + expr + ": "
            + cudaGetErrorString(error));
    }
}

#define RT_CUDA_CHECK(expr) throw_cuda_error((expr), #expr)

void free_device_ptr(void* ptr) {
    if (ptr != nullptr) {
        cudaFree(ptr);
    }
}

void free_frame_buffers(DeviceFrameBuffers& frame) {
    free_device_ptr(frame.beauty);
    free_device_ptr(frame.normal);
    free_device_ptr(frame.albedo);
    free_device_ptr(frame.depth);
    frame = DeviceFrameBuffers {};
}

void allocate_frame_buffers(DeviceFrameBuffers& frame, std::size_t pixel_count) {
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&frame.beauty), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&frame.normal), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&frame.albedo), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&frame.depth), pixel_count * sizeof(float)));
}

}  // namespace

DeviceFrameBufferSet::~DeviceFrameBufferSet() {
    reset_history();
    reset_frame();
}

void DeviceFrameBufferSet::resize_frame(int width, int height) {
    if (frame_width_ == width && frame_height_ == height) {
        return;
    }
    reset_frame();
    allocate_frame_buffers(frame_, static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    frame_width_ = width;
    frame_height_ = height;
}

void DeviceFrameBufferSet::reset_frame() {
    free_frame_buffers(frame_);
    frame_width_ = 0;
    frame_height_ = 0;
}

const DeviceFrameBuffers& DeviceFrameBufferSet::frame() const {
    return frame_;
}

int DeviceFrameBufferSet::frame_width() const {
    return frame_width_;
}

int DeviceFrameBufferSet::frame_height() const {
    return frame_height_;
}

void DeviceFrameBufferSet::resize_history(int width, int height) {
    if (width == history_width_ && height == history_height_ && history_.beauty != nullptr) {
        return;
    }
    reset_history();
    allocate_frame_buffers(history_, static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    history_width_ = width;
    history_height_ = height;
}

void DeviceFrameBufferSet::reset_history() {
    free_frame_buffers(history_);
    history_width_ = 0;
    history_height_ = 0;
    history_length_ = 0;
}

const DeviceFrameBuffers& DeviceFrameBufferSet::history() const {
    return history_;
}

int DeviceFrameBufferSet::history_width() const {
    return history_width_;
}

int DeviceFrameBufferSet::history_height() const {
    return history_height_;
}

LaunchHistoryState DeviceFrameBufferSet::history_state() const {
    LaunchHistoryState state {};
    state.buffers = history_;
    state.history_length = history_length_;
    for (int i = 0; i < 3; ++i) {
        state.prev_origin[i] = prev_origin_[i];
        state.prev_basis_x[i] = prev_basis_x_[i];
        state.prev_basis_y[i] = prev_basis_y_[i];
        state.prev_basis_z[i] = prev_basis_z_[i];
    }
    return state;
}

void DeviceFrameBufferSet::apply_history_state(const LaunchHistoryState& state) {
    history_length_ = state.history_length;
    for (int i = 0; i < 3; ++i) {
        prev_origin_[i] = state.prev_origin[i];
        prev_basis_x_[i] = state.prev_basis_x[i];
        prev_basis_y_[i] = state.prev_basis_y[i];
        prev_basis_z_[i] = state.prev_basis_z[i];
    }
}

void DeviceFrameBufferSet::reset_accumulation() {
    history_length_ = 0;
}

void DeviceFrameBufferSet::copy_frame_to_history(cudaStream_t stream) {
    const std::size_t pixel_count =
        static_cast<std::size_t>(history_width_) * static_cast<std::size_t>(history_height_);
    RT_CUDA_CHECK(cudaMemcpyAsync(
        history_.beauty, frame_.beauty, pixel_count * sizeof(float4), cudaMemcpyDeviceToDevice, stream));
    RT_CUDA_CHECK(cudaMemcpyAsync(
        history_.normal, frame_.normal, pixel_count * sizeof(float4), cudaMemcpyDeviceToDevice, stream));
    RT_CUDA_CHECK(cudaMemcpyAsync(
        history_.depth, frame_.depth, pixel_count * sizeof(float), cudaMemcpyDeviceToDevice, stream));
}

}  // namespace rt
