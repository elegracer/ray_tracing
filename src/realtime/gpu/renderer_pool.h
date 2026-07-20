#pragma once

#include "realtime/gpu/optix_renderer.h"

#include <cstdint>
#include <memory>
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

struct RendererPoolDiagnostics {
    int worker_count = 0;
    std::uint64_t worker_start_count = 0;
    std::uint64_t task_submission_count = 0;
    std::uint64_t launch_parameter_allocation_count = 0;
    std::uint64_t launch_parameter_upload_count = 0;
    AccelerationUpdateStats acceleration;
};

class RendererPool {
public:
    explicit RendererPool(int renderer_count);
    ~RendererPool();

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
    RendererPoolDiagnostics diagnostics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rt
