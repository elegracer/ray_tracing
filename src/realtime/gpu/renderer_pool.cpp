#include "realtime/gpu/renderer_pool.h"

#include <future>
#include <stdexcept>
#include <string>

namespace rt {

RendererPool::RendererPool(int renderer_count) {
    if (renderer_count < 1 || renderer_count > 4) {
        throw std::runtime_error("RendererPool requires renderer_count in [1, 4]");
    }
    renderers_.resize(static_cast<std::size_t>(renderer_count));
}

void RendererPool::prepare_scene(const PackedScene& scene) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (OptixRenderer& renderer : renderers_) {
        renderer.prepare_scene(scene);
    }
}

std::vector<CameraRenderResult> RendererPool::render_frame(
    const PackedCameraRig& rig, const RenderProfile& profile, int active_cameras) {
    std::lock_guard<std::mutex> lock(mutex_);
    validate_render_request(rig, active_cameras);

    std::vector<CameraRenderResult> results;
    results.reserve(static_cast<std::size_t>(active_cameras));

    std::vector<std::future<CameraRenderResult>> futures;
    futures.reserve(static_cast<std::size_t>(active_cameras));
    for (int camera_index = 0; camera_index < active_cameras; ++camera_index) {
        futures.push_back(std::async(std::launch::async, [this, &rig, &profile, camera_index]() {
            CameraRenderResult result {};
            result.camera_index = camera_index;
            result.profiled = renderers_[static_cast<std::size_t>(camera_index)]
                                  .render_prepared_radiance(rig, profile, camera_index);
            return result;
        }));
    }

    for (std::future<CameraRenderResult>& future : futures) {
        results.push_back(future.get());
    }
    return results;
}

void RendererPool::validate_render_request(const PackedCameraRig& rig, int active_cameras) const {
    if (active_cameras < 1 || active_cameras > static_cast<int>(renderers_.size())) {
        throw std::runtime_error("RendererPool active_cameras out of range");
    }
    if (rig.active_count < 1 || rig.active_count > 4) {
        throw std::runtime_error("RendererPool rig.active_count out of range");
    }
    if (active_cameras > rig.active_count) {
        throw std::runtime_error("RendererPool active_cameras exceeds rig.active_count");
    }

    for (int camera_index = 0; camera_index < active_cameras; ++camera_index) {
        const PackedCamera& camera = rig.cameras[static_cast<std::size_t>(camera_index)];
        if (camera.enabled == 0) {
            throw std::runtime_error("RendererPool leading camera slot disabled at index "
                + std::to_string(camera_index));
        }
        if (camera.width <= 0 || camera.height <= 0) {
            throw std::runtime_error("RendererPool leading camera slot has invalid resolution at index "
                + std::to_string(camera_index));
        }
    }
}

}  // namespace rt
