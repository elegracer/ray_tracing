#pragma once

#include "realtime/gpu/radiance_launch_setup.h"

#include <cuda_runtime.h>

namespace rt {

class DeviceFrameBufferSet {
   public:
    DeviceFrameBufferSet() = default;
    ~DeviceFrameBufferSet();

    DeviceFrameBufferSet(const DeviceFrameBufferSet&) = delete;
    DeviceFrameBufferSet& operator=(const DeviceFrameBufferSet&) = delete;
    DeviceFrameBufferSet(DeviceFrameBufferSet&&) = delete;
    DeviceFrameBufferSet& operator=(DeviceFrameBufferSet&&) = delete;

    void resize_frame(int width, int height);
    void reset_frame();
    const DeviceFrameBuffers& frame() const;
    int frame_width() const;
    int frame_height() const;

    void resize_history(int width, int height);
    void reset_history();
    const DeviceFrameBuffers& history() const;
    int history_width() const;
    int history_height() const;

    LaunchHistoryState history_state() const;
    void apply_history_state(const LaunchHistoryState& state);
    void reset_accumulation();
    void copy_frame_to_history(cudaStream_t stream);

   private:
    DeviceFrameBuffers frame_ {};
    int frame_width_ = 0;
    int frame_height_ = 0;
    DeviceFrameBuffers history_ {};
    int history_width_ = 0;
    int history_height_ = 0;
    int history_length_ = 0;
    double prev_origin_[3] {};
    double prev_basis_x_[3] {};
    double prev_basis_y_[3] {};
    double prev_basis_z_[3] {};
};

}  // namespace rt
