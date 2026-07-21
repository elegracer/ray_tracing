#include "realtime/gpu/radiance_launch_setup.h"
#include "realtime/render_profile.h"
#include "test_support.h"

int main() {
    rt::PackedScene scene;
    scene.background = Eigen::Vector3d {0.1, 0.2, 0.3};
    scene.spheres.resize(2);
    scene.quads.resize(3);
    scene.triangles.resize(4);
    scene.media.resize(5);
    scene.textures.resize(6);
    scene.materials.resize(7);
    scene.analytic_lights.resize(10);

    rt::DeviceSceneView scene_view {};
    scene_view.spheres = reinterpret_cast<rt::PackedSphere*>(0x10);
    scene_view.quads = reinterpret_cast<rt::PackedQuad*>(0x20);
    scene_view.triangles = reinterpret_cast<rt::PackedTriangle*>(0x30);
    scene_view.media = reinterpret_cast<rt::PackedMedium*>(0x40);
    scene_view.textures = reinterpret_cast<rt::PackedTexture*>(0x50);
    scene_view.image_texels = reinterpret_cast<Eigen::Vector3f*>(0x60);
    scene_view.materials = reinterpret_cast<rt::MaterialSample*>(0x70);
    scene_view.image_texel_count = 8;

    rt::PackedCameraRig rig;
    rig.active_count = 1;
    rt::PackedCamera& camera = rig.cameras[0];
    camera.enabled = 1;
    camera.width = 64;
    camera.height = 48;
    camera.T_rc.translation() = Eigen::Vector3d {1.0, 2.0, 3.0};
    camera.pinhole = rt::Pinhole32Params {100.0, 101.0, 32.0, 24.0, 0.1, 0.2, 0.3, 0.4, 0.5};

    rt::RenderProfile profile {};
    profile.samples_per_pixel = 9;
    profile.max_bounces = 10;
    profile.rr_start_bounce = 3;
    profile.enable_restir_di = true;
    profile.restir_initial_candidates = 7;
    profile.restir_temporal_reuse = true;
    profile.restir_max_history_age = 12;
    profile.restir_max_temporal_candidates = 48;
    profile.restir_spatial_neighbors = 3;
    profile.restir_max_spatial_candidates = 2;
    profile.restir_min_analytic_lights = 10;

    rt::DeviceFrameBuffers frame {};
    frame.beauty = reinterpret_cast<float4*>(0x80);
    frame.normal = reinterpret_cast<float4*>(0x90);
    frame.albedo = reinterpret_cast<float4*>(0xa0);
    frame.depth = reinterpret_cast<float*>(0xb0);

    rt::DeviceFrameBuffers history {};
    history.beauty = reinterpret_cast<float4*>(0xc0);
    history.normal = reinterpret_cast<float4*>(0xd0);
    history.albedo = reinterpret_cast<float4*>(0xe0);
    history.depth = reinterpret_cast<float*>(0xf0);

    rt::LaunchHistoryState history_state {};
    history_state.buffers = history;
    history_state.previous_camera = rt::make_device_active_camera(camera);
    history_state.previous_camera.origin[0] = 10.0;
    history_state.previous_camera_valid = 1;
    history_state.history_length = 4;
    history_state.prev_origin[0] = 10.0;
    history_state.prev_origin[1] = 11.0;
    history_state.prev_origin[2] = 12.0;
    history_state.prev_basis_x[0] = 1.0;
    history_state.prev_basis_y[1] = 1.0;
    history_state.prev_basis_z[2] = 1.0;

    const rt::LaunchParams params = rt::make_radiance_launch_params(scene, scene_view, rig, profile,
        0, 42, frame, history_state);

    expect_true(params.width == 64, "width from camera");
    expect_true(params.height == 48, "height from camera");
    expect_true(params.sample_stream == 42, "sample stream");
    expect_true(params.samples_per_pixel == 9, "spp");
    expect_true(params.max_bounces == 10, "max bounces");
    expect_true(params.rr_start_bounce == 3, "rr start");
    expect_true(params.mode == 1, "mode");
    expect_true(params.restir_di_enabled == 1, "ReSTIR enabled");
    expect_true(params.restir_initial_candidates == 7, "ReSTIR candidate count");
    expect_true(params.restir_temporal_reuse == 1, "ReSTIR temporal reuse");
    expect_true(params.restir_max_history_age == 12, "ReSTIR history age");
    expect_true(params.restir_max_temporal_candidates == 48, "ReSTIR temporal M clamp");
    expect_true(params.restir_spatial_neighbors == 3, "ReSTIR spatial neighbor count");
    expect_true(params.restir_max_spatial_candidates == 2, "ReSTIR spatial M clamp");
    expect_true(params.restir_min_analytic_lights == 10, "ReSTIR many-light threshold");
    rt::PackedScene sparse_scene = scene;
    sparse_scene.analytic_lights.resize(9);
    const rt::LaunchParams sparse_params = rt::make_radiance_launch_params(sparse_scene, scene_view,
        rig, profile, 0, 42, frame, history_state);
    expect_true(sparse_params.restir_di_enabled == 0,
        "ReSTIR stays off below the configured many-light threshold");
    expect_near(params.background[0], 0.1, 1e-6, "background r");
    expect_near(params.background[1], 0.2, 1e-6, "background g");
    expect_near(params.background[2], 0.3, 1e-6, "background b");

    expect_true(params.frame.beauty == frame.beauty, "frame beauty");
    expect_true(params.scene.spheres == scene_view.spheres, "scene spheres ptr");
    expect_true(params.scene.quads == scene_view.quads, "scene quads ptr");
    expect_true(params.scene.triangles == scene_view.triangles, "scene triangles ptr");
    expect_true(params.scene.media == scene_view.media, "scene media ptr");
    expect_true(params.scene.textures == scene_view.textures, "scene textures ptr");
    expect_true(params.scene.image_texels == scene_view.image_texels, "scene texels ptr");
    expect_true(params.scene.materials == scene_view.materials, "scene materials ptr");
    expect_true(params.scene.sphere_count == 2, "sphere count");
    expect_true(params.scene.quad_count == 3, "quad count");
    expect_true(params.scene.triangle_count == 4, "triangle count");
    expect_true(params.scene.medium_count == 5, "medium count");
    expect_true(params.scene.texture_count == 6, "texture count");
    expect_true(params.scene.material_count == 7, "material count");
    expect_true(params.scene.image_texel_count == 8, "image texel count");

    expect_near(params.active_camera.origin[0], 1.0, 1e-12, "camera origin x");
    expect_near(params.active_camera.origin[1], 2.0, 1e-12, "camera origin y");
    expect_near(params.active_camera.origin[2], 3.0, 1e-12, "camera origin z");
    expect_near(params.active_camera.pinhole.fx, 100.0, 1e-12, "camera fx");
    expect_near(params.active_camera.pinhole.p2, 0.5, 1e-12, "camera p2");

    expect_true(params.history.beauty == history.beauty, "history beauty");
    expect_true(params.history_length == 4, "history length");
    expect_true(params.previous_camera_valid == 1, "previous camera valid");
    expect_near(params.previous_camera.origin[0], 10.0, 1e-12, "previous camera origin");
    expect_near(params.prev_origin[1], 11.0, 1e-12, "prev origin");
    expect_near(params.prev_basis_y[1], 1.0, 1e-12, "prev basis y");

    const rt::LaunchHistoryState next_history = rt::capture_launch_history(params);
    expect_true(next_history.history_length == 5, "next history length increments");
    expect_true(next_history.previous_camera_valid == 1, "captured previous camera valid");
    expect_near(next_history.previous_camera.origin[0], 1.0, 1e-12,
        "captured previous camera origin");
    expect_near(next_history.prev_origin[2], 3.0, 1e-12, "next prev origin");
    expect_near(next_history.prev_basis_z[2], 1.0, 1e-12, "next prev basis z");

    rt::LaunchHistoryState empty_history {};
    const rt::LaunchParams first_params = rt::make_radiance_launch_params(scene, scene_view, rig,
        profile, 0, 7, frame, empty_history);
    const rt::LaunchHistoryState first_next = rt::capture_launch_history(first_params);
    expect_true(first_next.history_length == 1, "first history length becomes one");

    return 0;
}
