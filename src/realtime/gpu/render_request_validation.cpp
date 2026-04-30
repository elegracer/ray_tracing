#include "realtime/gpu/render_request_validation.h"

#include <stdexcept>
#include <string>

namespace rt {
namespace {

std::string caller_name(std::string_view caller) {
    return std::string(caller);
}

}  // namespace

void validate_render_camera_request(
    const PackedCameraRig& rig, int camera_index, std::string_view caller) {
    constexpr int kMaxCameraSlots = 4;
    const std::string name = caller_name(caller);
    if (rig.active_count < 1 || rig.active_count > kMaxCameraSlots) {
        throw std::runtime_error(name + " requires rig.active_count in [1, 4], got "
            + std::to_string(rig.active_count));
    }
    if (camera_index < 0 || camera_index >= rig.active_count) {
        throw std::runtime_error(name + " camera_index out of range: camera_index="
            + std::to_string(camera_index) + ", active_count=" + std::to_string(rig.active_count));
    }

    const PackedCamera& camera = rig.cameras[static_cast<std::size_t>(camera_index)];
    if (camera.enabled == 0) {
        throw std::runtime_error(name + " camera slot is disabled at index "
            + std::to_string(camera_index));
    }
    if (camera.width <= 0 || camera.height <= 0) {
        throw std::runtime_error(name + " camera slot has invalid resolution at index "
            + std::to_string(camera_index) + ": width=" + std::to_string(camera.width) + ", height="
            + std::to_string(camera.height));
    }
}

void validate_render_pool_request(
    const PackedCameraRig& rig, int active_cameras, int renderer_count, std::string_view caller) {
    const std::string name = caller_name(caller);
    if (active_cameras < 1 || active_cameras > renderer_count) {
        throw std::runtime_error(name + " active_cameras out of range");
    }
    if (rig.active_count < 1 || rig.active_count > 4) {
        throw std::runtime_error(name + " rig.active_count out of range");
    }
    if (active_cameras > rig.active_count) {
        throw std::runtime_error(name + " active_cameras exceeds rig.active_count");
    }

    for (int camera_index = 0; camera_index < active_cameras; ++camera_index) {
        const PackedCamera& camera = rig.cameras[static_cast<std::size_t>(camera_index)];
        if (camera.enabled == 0) {
            throw std::runtime_error(name + " leading camera slot disabled at index "
                + std::to_string(camera_index));
        }
        if (camera.width <= 0 || camera.height <= 0) {
            throw std::runtime_error(name + " leading camera slot has invalid resolution at index "
                + std::to_string(camera_index));
        }
    }
}

}  // namespace rt
