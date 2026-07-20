#include "realtime/gpu/packed_scene_preparation.h"
#include "scene/analytic_light_compiler.h"
#include "scene/cpu_scene_adapter.h"
#include "scene/realtime_scene_adapter.h"
#include "test_support.h"

#include <Eigen/Geometry>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace {

rt::scene::ScenePrim light_prim(std::string path, rt::scene::SceneLight light,
    const Eigen::Matrix4d& transform = Eigen::Matrix4d::Identity()) {
    return rt::scene::ScenePrim {
        .path = std::move(path),
        .kind = rt::scene::ScenePrimKind::light,
        .local_to_parent = transform,
        .light = std::move(light),
    };
}

Eigen::Matrix4d scaled_translation(const Eigen::Vector3d& scale,
    const Eigen::Vector3d& translation = Eigen::Vector3d::Zero()) {
    Eigen::Matrix4d transform = Eigen::Matrix4d::Identity();
    transform.block<3, 3>(0, 0) = scale.asDiagonal();
    transform.block<3, 1>(0, 3) = translation;
    return transform;
}

rt::scene::SceneIRv2 make_light_scene() {
    rt::scene::SceneIRv2 scene;
    scene.stage_metadata().default_prim_path = "/World";
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World",
        .kind = rt::scene::ScenePrimKind::scope,
    });

    rt::scene::SceneLight sphere;
    sphere.type = rt::scene::SceneLightType::sphere;
    sphere.radius = 0.5;
    sphere.intensity = 4.0;
    sphere.normalize = true;
    scene.add_prim(light_prim("/World/Sphere", sphere,
        scaled_translation(Eigen::Vector3d::Constant(2.0), Eigen::Vector3d {1.0, 2.0, 3.0})));

    rt::scene::SceneLight disk;
    disk.type = rt::scene::SceneLightType::disk;
    disk.radius = 1.0;
    disk.intensity = 6.0 * std::numbers::pi;
    disk.normalize = true;
    scene.add_prim(
        light_prim("/World/Disk", disk, scaled_translation(Eigen::Vector3d {2.0, 3.0, 1.0})));

    rt::scene::SceneLight rect;
    rect.type = rt::scene::SceneLightType::rect;
    rect.width = 2.0;
    rect.height = 3.0;
    rect.intensity = 6.0;
    rect.normalize = true;
    scene.add_prim(light_prim("/World/Rect", rect));

    rt::scene::SceneLight cylinder;
    cylinder.type = rt::scene::SceneLightType::cylinder;
    cylinder.radius = 1.0;
    cylinder.length = 2.0;
    cylinder.intensity = 16.0 * std::numbers::pi;
    cylinder.normalize = true;
    scene.add_prim(light_prim("/World/Cylinder", cylinder,
        scaled_translation(Eigen::Vector3d::Constant(2.0))));

    rt::scene::SceneLight distant;
    distant.type = rt::scene::SceneLightType::distant;
    distant.angle_degrees = 60.0;
    distant.intensity = std::numbers::pi / 4.0;
    distant.normalize = true;
    scene.add_prim(light_prim("/World/Distant", distant));

    rt::scene::SceneLight dome;
    dome.type = rt::scene::SceneLightType::dome;
    dome.intensity = 2.0;
    dome.exposure = 1.0;
    scene.add_prim(light_prim("/World/Dome", dome));

    rt::scene::SceneLight hidden;
    hidden.type = rt::scene::SceneLightType::sphere;
    rt::scene::ScenePrim hidden_prim = light_prim("/World/Hidden", hidden);
    hidden_prim.visibility = rt::scene::SceneVisibility::invisible;
    scene.add_prim(std::move(hidden_prim));

    rt::scene::SceneLight proxy;
    proxy.type = rt::scene::SceneLightType::sphere;
    rt::scene::ScenePrim proxy_prim = light_prim("/World/Proxy", proxy);
    proxy_prim.authored_purpose = rt::scene::ScenePurpose::proxy;
    scene.add_prim(std::move(proxy_prim));
    return scene;
}

void expect_compile_failure(rt::scene::ScenePrim prim, const std::string& message_fragment) {
    rt::scene::SceneIRv2 scene;
    scene.stage_metadata().default_prim_path = "/World";
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World",
        .kind = rt::scene::ScenePrimKind::scope,
    });
    scene.add_prim(std::move(prim));
    try {
        (void)rt::scene::compile_analytic_lights(scene);
    } catch (const std::invalid_argument& error) {
        expect_true(std::string {error.what()}.contains(message_fragment),
            "analytic-light failure reports the unsupported contract");
        return;
    }
    throw std::runtime_error("expected analytic-light compilation to fail");
}

} // namespace

int main() {
    const rt::scene::SceneIRv2 scene = make_light_scene();
    const std::vector<rt::AnalyticLightDesc> lights = rt::scene::compile_analytic_lights(scene);
    expect_true(lights.size() == 6, "only visible render-purpose intrinsic lights compile");

    const rt::AnalyticLightDesc& sphere = lights[0];
    expect_true(sphere.type == rt::AnalyticLightType::sphere, "sphere type");
    expect_vec3_near(sphere.position, Eigen::Vector3d {1.0, 2.0, 3.0}, 1e-12,
        "sphere world position");
    expect_near(sphere.world_area, 4.0 * std::numbers::pi, 1e-12, "sphere world-space area");
    expect_vec3_near(sphere.radiance, Eigen::Vector3d::Constant(1.0 / std::numbers::pi), 1e-12,
        "normalized sphere radiance");

    expect_near(lights[1].world_area, 6.0 * std::numbers::pi, 1e-12,
        "affine disk world-space area");
    expect_vec3_near(lights[1].radiance, Eigen::Vector3d::Ones(), 1e-12,
        "normalized disk radiance");
    expect_near(lights[2].world_area, 6.0, 1e-12, "rect world-space area");
    expect_near(lights[3].world_area, 16.0 * std::numbers::pi, 1e-12,
        "cylinder lateral area excludes end caps");
    expect_near(lights[4].cos_theta_max, std::cos(std::numbers::pi / 6.0), 1e-12,
        "distant angular radius");
    expect_vec3_near(lights[4].radiance, Eigen::Vector3d::Ones(), 1e-12,
        "normalized distant radiance");
    expect_vec3_near(lights[5].radiance, Eigen::Vector3d::Constant(4.0), 1e-12,
        "dome exposure radiance");

    double probability_sum = 0.0;
    for (const rt::AnalyticLightDesc& light : lights) {
        expect_true(light.selection_pdf > 0.0, "compiled light has positive selection PDF");
        probability_sum += light.selection_pdf;
    }
    expect_near(probability_sum, 1.0, 1e-12, "analytic-light selection PDFs normalize");
    expect_near(lights.back().cdf, 1.0, 1e-12, "analytic-light CDF closes exactly");

    const rt::scene::SceneIR compatibility;
    const rt::scene::CpuSceneAdapterResult cpu =
        rt::scene::adapt_to_cpu_openpbr(compatibility, scene);
    expect_true(cpu.analytic_lights.size() == lights.size(),
        "CPU adapter carries the analytic-light table");
    const rt::PackedScene realtime =
        rt::scene::adapt_to_realtime_openpbr(compatibility, scene).pack();
    expect_true(realtime.analytic_light_count == static_cast<int>(lights.size()),
        "realtime scene count includes analytic lights");
    expect_true(realtime.analytic_lights.size() == lights.size(),
        "realtime scene carries the analytic-light table");
    const rt::GpuPreparedScene gpu = rt::prepare_gpu_scene(realtime);
    expect_true(gpu.analytic_lights.size() == lights.size(),
        "GPU preparation packs every analytic light");
    expect_near(gpu.analytic_lights[0].world_area, sphere.world_area, 1e-5,
        "GPU packed sphere area");
    expect_near(gpu.analytic_lights[4].cos_theta_max, lights[4].cos_theta_max, 1e-6,
        "GPU packed distant cone");
    expect_near(gpu.analytic_lights.back().cdf, 1.0, 1e-6,
        "GPU packed analytic CDF closes exactly");

    rt::scene::SceneLight warm;
    warm.type = rt::scene::SceneLightType::sphere;
    warm.enable_color_temperature = true;
    warm.color_temperature_kelvin = 3200.0;
    rt::scene::SceneIRv2 warm_scene;
    warm_scene.stage_metadata().default_prim_path = "/World";
    warm_scene.add_prim(rt::scene::ScenePrim {
        .path = "/World",
        .kind = rt::scene::ScenePrimKind::scope,
    });
    warm_scene.add_prim(light_prim("/World/Warm", warm));
    const Eigen::Vector3d warm_radiance =
        rt::scene::compile_analytic_lights(warm_scene).front().radiance;
    expect_true(warm_radiance.x() > warm_radiance.y() && warm_radiance.y() > warm_radiance.z(),
        "OpenUSD blackbody color temperature warms red over blue");
    expect_near(warm_radiance.dot(Eigen::Vector3d {0.2126, 0.7152, 0.0722}), 1.0, 1e-6,
        "OpenUSD blackbody color temperature preserves luminance");

    rt::scene::SceneLight textured_dome;
    textured_dome.type = rt::scene::SceneLightType::dome;
    rt::scene::ScenePrim textured = light_prim("/World/Textured", textured_dome);
    textured.asset_references.push_back(rt::scene::SceneAssetReference {
        .authored_path = "studio.exr",
    });
    expect_compile_failure(std::move(textured), "textures are not supported yet");

    rt::scene::SceneLight stretched_sphere;
    stretched_sphere.type = rt::scene::SceneLightType::sphere;
    expect_compile_failure(light_prim("/World/Stretched", stretched_sphere,
                               scaled_translation(Eigen::Vector3d {1.0, 2.0, 1.0})),
        "similarity transform");

    rt::scene::SceneLight response_override;
    response_override.type = rt::scene::SceneLightType::rect;
    response_override.diffuse = 0.5;
    expect_compile_failure(light_prim("/World/Response", response_override),
        "diffuse/specular overrides");
    return 0;
}
