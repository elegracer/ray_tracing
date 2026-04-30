#pragma once

#include "realtime/camera_rig.h"
#include "realtime/gpu/launch_params.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"

namespace rt {

struct LaunchHistoryState {
    DeviceFrameBuffers buffers {};
    int history_length = 0;
    double prev_origin[3] {};
    double prev_basis_x[3] {};
    double prev_basis_y[3] {};
    double prev_basis_z[3] {};
};

DeviceActiveCamera make_device_active_camera(const PackedCamera& camera);

LaunchParams make_radiance_launch_params(const PackedScene& scene, const DeviceSceneView& scene_view,
    const PackedCameraRig& rig, const RenderProfile& profile, int camera_index, std::uint32_t sample_stream,
    DeviceFrameBuffers frame, const LaunchHistoryState& history);

LaunchHistoryState capture_launch_history(const LaunchParams& params);

}  // namespace rt
