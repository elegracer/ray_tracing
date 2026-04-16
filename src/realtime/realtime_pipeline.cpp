#include "realtime/realtime_pipeline.h"

#include <cmath>
#include <stdexcept>

namespace rt {

namespace {

void validate_active_cameras(int active_cameras) {
    if (active_cameras < 1 || active_cameras > 4) {
        throw std::runtime_error("realtime pipeline supports 1..4 active cameras");
    }
}

PackedScene make_smoke_scene() {
    SceneDescription scene;
    const int light = scene.add_material(DiffuseLightMaterial {Eigen::Vector3d {8.0, 8.0, 8.0}});
    const int smoke = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.8, 0.8, 0.8}});

    scene.add_quad(QuadPrimitive {
        light,
        Eigen::Vector3d {-1.0, 1.5, -3.5},
        Eigen::Vector3d {2.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, -2.0},
        false,
    });
    scene.add_sphere(SpherePrimitive {smoke, Eigen::Vector3d {0.0, 0.0, -4.0}, 0.7, false});
    return scene.pack();
}

PackedCameraRig make_smoke_rig(int active_cameras, double pose_jump_translation, std::array<double, 4>& pose_x) {
    CameraRig rig;
    const Pinhole32Params intrinsics {200.0, 200.0, 32.0, 32.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    constexpr int kWidth = 64;
    constexpr int kHeight = 64;
    const double center = 0.5 * static_cast<double>(active_cameras - 1);

    for (int i = 0; i < active_cameras; ++i) {
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
        T_bc.translation() = Eigen::Vector3d {0.06 * (static_cast<double>(i) - center) + pose_jump_translation, 0.0, 0.0};
        pose_x[static_cast<std::size_t>(i)] = T_bc.translation().x();
        rig.add_pinhole(intrinsics, T_bc, kWidth, kHeight);
    }

    return rig.pack();
}

}  // namespace

RealtimeFrameSet RealtimePipeline::render_profiled_smoke_frame(
    int active_cameras, const RenderProfile& profile) {
    return render_profiled_smoke_frame_impl(active_cameras, profile, false);
}

RealtimeFrameSet RealtimePipeline::render_profiled_smoke_frame_with_pose_jump(
    int active_cameras, const RenderProfile& profile) {
    return render_profiled_smoke_frame_impl(active_cameras, profile, true);
}

RealtimeFrameSet RealtimePipeline::render_profiled_smoke_frame_impl(
    int active_cameras, const RenderProfile& profile, bool pose_jump) {
    validate_active_cameras(active_cameras);

    const PackedScene scene = make_smoke_scene();
    std::array<double, 4> pose_x {};
    const double pose_jump_translation = pose_jump ? profile.accumulation_reset_translation * 2.0 : 0.0;
    const PackedCameraRig rig = make_smoke_rig(active_cameras, pose_jump_translation, pose_x);

    RealtimeFrameSet out {};
    out.frames.resize(static_cast<std::size_t>(active_cameras));
    for (int i = 0; i < active_cameras; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        if (pose_initialized_[idx] != 0
            && std::abs(pose_x[idx] - last_pose_x_[idx]) > profile.accumulation_reset_translation) {
            history_lengths_[idx] = 0;
        }

        last_pose_x_[idx] = pose_x[idx];
        pose_initialized_[idx] = 1;
        history_lengths_[idx] += 1;

        RealtimeFrame frame {};
        frame.history_length = history_lengths_[idx];
        frame.radiance = renderer_.render_radiance(scene, rig, profile, i);
        if (profile.enable_denoise) {
            denoiser_.run(frame.radiance);
        }
        out.frames[idx] = frame;
    }
    return out;
}

}  // namespace rt
