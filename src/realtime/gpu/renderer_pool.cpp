#include "realtime/gpu/renderer_pool.h"

#include <future>
#include <stdexcept>

namespace rt {

RendererPool::RendererPool(int renderer_count) {
    if (renderer_count < 1 || renderer_count > 4) {
        throw std::runtime_error("RendererPool requires renderer_count in [1, 4]");
    }
    renderers_.resize(static_cast<std::size_t>(renderer_count));
}

void RendererPool::prepare_scene(const PackedScene& scene) {
    for (OptixRenderer& renderer : renderers_) {
        renderer.prepare_scene(scene);
    }
}

std::vector<CameraRenderResult> RendererPool::render_frame(
    const PackedCameraRig& rig, const RenderProfile& profile, int active_cameras) {
    if (active_cameras < 1 || active_cameras > static_cast<int>(renderers_.size())) {
        throw std::runtime_error("RendererPool active_cameras out of range");
    }

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

    std::vector<CameraRenderResult> results;
    results.reserve(static_cast<std::size_t>(active_cameras));
    for (std::future<CameraRenderResult>& future : futures) {
        results.push_back(future.get());
    }
    return results;
}

}  // namespace rt
