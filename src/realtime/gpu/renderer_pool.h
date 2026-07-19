#pragma once

#include "realtime/gpu/optix_renderer.h"

#include <deque>
#include <mutex>
#include <vector>

namespace rt {

struct CameraRenderResult {
    int camera_index = 0;
    ProfiledRadianceFrame profiled;
};

struct CameraDeviceRenderResult {
    int camera_index = 0;
    ProfiledDeviceRadianceFrame profiled;
};

class RendererPool {
public:
    explicit RendererPool(int renderer_count);
    ~RendererPool() = default;

    RendererPool(const RendererPool&) = delete;
    RendererPool& operator=(const RendererPool&) = delete;
    RendererPool(RendererPool&&) = delete;
    RendererPool& operator=(RendererPool&&) = delete;

    void prepare_scene(const PackedScene& scene);
    void reset_accumulation();
    void reset_sequence(std::uint32_t sample_stream);
    // Device views are consumed before the next pool operation invalidates renderer outputs.
    std::vector<CameraDeviceRenderResult> render_device_frame(const PackedCameraRig& rig,
        const RenderProfile& profile, int active_cameras);
    std::vector<CameraRenderResult> render_frame(const PackedCameraRig& rig,
        const RenderProfile& profile, int active_cameras);

private:
    std::deque<OptixRenderer> renderers_;
    mutable std::mutex mutex_;
};

} // namespace rt
