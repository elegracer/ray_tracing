#include "realtime/gpu/radiance_launch_setup.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace rt {
namespace {

int checked_scene_count(std::size_t count, const char* label) {
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error(std::string("scene ") + label + " count exceeds int range");
    }
    return static_cast<int>(count);
}

}  // namespace

DeviceActiveCamera make_device_active_camera(const PackedCamera& camera) {
    DeviceActiveCamera active {};
    active.width = camera.width;
    active.height = camera.height;
    active.model = camera.model;

    const Eigen::Vector3d origin = camera.T_rc.translation();
    const Eigen::Matrix3d rotation = camera.T_rc.rotationMatrix();
    active.origin[0] = origin.x();
    active.origin[1] = origin.y();
    active.origin[2] = origin.z();

    for (int row = 0; row < 3; ++row) {
        active.basis_x[row] = rotation(row, 0);
        active.basis_y[row] = rotation(row, 1);
        active.basis_z[row] = rotation(row, 2);
    }

    active.pinhole.fx = camera.pinhole.fx;
    active.pinhole.fy = camera.pinhole.fy;
    active.pinhole.cx = camera.pinhole.cx;
    active.pinhole.cy = camera.pinhole.cy;
    active.pinhole.k1 = camera.pinhole.k1;
    active.pinhole.k2 = camera.pinhole.k2;
    active.pinhole.k3 = camera.pinhole.k3;
    active.pinhole.p1 = camera.pinhole.p1;
    active.pinhole.p2 = camera.pinhole.p2;

    active.equi.width = camera.equi.width;
    active.equi.height = camera.equi.height;
    active.equi.fx = camera.equi.fx;
    active.equi.fy = camera.equi.fy;
    active.equi.cx = camera.equi.cx;
    active.equi.cy = camera.equi.cy;
    active.equi.tangential[0] = camera.equi.tangential.x();
    active.equi.tangential[1] = camera.equi.tangential.y();
    active.equi.lut_step = camera.equi.lut_step;
    for (int i = 0; i < 6; ++i) {
        active.equi.radial[i] = camera.equi.radial[static_cast<std::size_t>(i)];
    }
    for (int i = 0; i < 1024; ++i) {
        active.equi.lut[i] = camera.equi.lut[static_cast<std::size_t>(i)];
    }

    return active;
}

LaunchParams make_radiance_launch_params(const PackedScene& scene, const DeviceSceneView& scene_view,
    const PackedCameraRig& rig, const RenderProfile& profile, int camera_index, std::uint32_t sample_stream,
    DeviceFrameBuffers frame, const LaunchHistoryState& history) {
    const PackedCamera& camera = rig.cameras[static_cast<std::size_t>(camera_index)];

    LaunchParams params {};
    params.width = camera.width;
    params.height = camera.height;
    params.sample_stream = sample_stream;
    params.active_camera = make_device_active_camera(camera);
    params.background[0] = static_cast<float>(scene.background.x());
    params.background[1] = static_cast<float>(scene.background.y());
    params.background[2] = static_cast<float>(scene.background.z());
    params.frame = frame;
    params.scene = scene_view;
    params.scene.sphere_count = checked_scene_count(scene.spheres.size(), "sphere");
    params.scene.quad_count = checked_scene_count(scene.quads.size(), "quad");
    params.scene.triangle_count = checked_scene_count(scene.triangles.size(), "triangle");
    params.scene.medium_count = checked_scene_count(scene.media.size(), "medium");
    params.scene.texture_count = checked_scene_count(scene.textures.size(), "texture");
    params.scene.material_count = checked_scene_count(scene.materials.size(), "material");
    params.samples_per_pixel = profile.samples_per_pixel;
    params.max_bounces = profile.max_bounces;
    params.rr_start_bounce = profile.rr_start_bounce;
    params.mode = 1;

    params.history = history.buffers;
    params.history_length = history.history_length;
    for (int i = 0; i < 3; ++i) {
        params.prev_origin[i] = history.prev_origin[i];
        params.prev_basis_x[i] = history.prev_basis_x[i];
        params.prev_basis_y[i] = history.prev_basis_y[i];
        params.prev_basis_z[i] = history.prev_basis_z[i];
    }

    return params;
}

LaunchHistoryState capture_launch_history(const LaunchParams& params) {
    LaunchHistoryState history {};
    history.buffers = params.history;
    history.history_length = params.history_length == 0 ? 1 : params.history_length + 1;
    const DeviceActiveCamera& cam = params.active_camera;
    for (int i = 0; i < 3; ++i) {
        history.prev_origin[i] = cam.origin[i];
        history.prev_basis_x[i] = cam.basis_x[i];
        history.prev_basis_y[i] = cam.basis_y[i];
        history.prev_basis_z[i] = cam.basis_z[i];
    }
    return history;
}

}  // namespace rt
