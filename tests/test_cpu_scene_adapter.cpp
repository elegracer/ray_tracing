#include "scene/cpu_scene_adapter.h"
#include "scene/shared_scene_builders.h"
#include "common/interval.h"
#include "common/material.h"
#include "common/ray.h"
#include "test_support.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace {

template <typename VariantType, typename AlternativeType>
bool has_variant(const std::vector<VariantType>& values) {
    return std::any_of(values.begin(), values.end(),
        [](const VariantType& value) { return std::holds_alternative<AlternativeType>(value); });
}

}  // namespace

int main() {
    constexpr std::array<std::string_view, 5> scene_ids {
        "bouncing_spheres",
        "checkered_spheres",
        "earth_sphere",
        "cornell_smoke",
        "perlin_spheres",
    };

    for (const std::string_view scene_id : scene_ids) {
        const rt::scene::SceneIR scene = rt::scene::build_scene(scene_id);
        const rt::scene::CpuSceneAdapterResult adapted = rt::scene::adapt_to_cpu(scene);
        expect_true(adapted.world.has_value(), "adapted world should be non-null");
    }

    const rt::scene::SceneIR perlin_scene = rt::scene::build_scene("perlin_spheres");
    expect_true(has_variant<rt::scene::TextureDesc, rt::scene::NoiseTextureDesc>(perlin_scene.textures()),
        "perlin_spheres should include noise texture");
    const rt::scene::CpuSceneAdapterResult adapted_perlin = rt::scene::adapt_to_cpu(perlin_scene);
    expect_true(adapted_perlin.world.has_value(), "perlin_spheres should adapt to world");
    const Ray perlin_ray {Vec3d {0.0, 2.0, -6.0}, Vec3d {0.0, 0.0, 1.0}};
    HitRecord perlin_hit;
    expect_true(adapted_perlin.world->hit(perlin_ray, Interval {0.001, infinity}, perlin_hit),
        "perlin_spheres ray should hit diffuse sphere");
    ScatterRecord perlin_scatter;
    expect_true(perlin_hit.mat->scatter(perlin_ray, perlin_hit, perlin_scatter),
        "perlin_spheres diffuse material should scatter");
    expect_true(!perlin_scatter.skip_pdf, "perlin_spheres diffuse scatter should not skip pdf");
    expect_true(perlin_scatter.pdf != nullptr, "perlin_spheres diffuse scatter should provide pdf");
    expect_true(perlin_hit.mat->scattering_pdf(perlin_ray, perlin_hit, Ray {perlin_hit.p, perlin_hit.normal}) > 0.0,
        "perlin_spheres diffuse scattering pdf should be positive");

    const rt::scene::SceneIR checkered_scene = rt::scene::build_scene("checkered_spheres");
    expect_true(has_variant<rt::scene::TextureDesc, rt::scene::CheckerTextureDesc>(checkered_scene.textures()),
        "checkered_spheres should include checker texture");
    const rt::scene::CpuSceneAdapterResult adapted_checkered = rt::scene::adapt_to_cpu(checkered_scene);
    expect_true(adapted_checkered.world.has_value(), "checkered_spheres should adapt to world");
    const Ray checker_ray {Vec3d {0.0, 10.0, -30.0}, Vec3d {0.0, 0.0, 1.0}};
    HitRecord checker_hit;
    expect_true(adapted_checkered.world->hit(checker_ray, Interval {0.001, infinity}, checker_hit),
        "checkered_spheres ray should hit top checker sphere");
    ScatterRecord checker_scatter;
    expect_true(checker_hit.mat->scatter(checker_ray, checker_hit, checker_scatter),
        "checkered_spheres diffuse material should scatter");
    expect_true(!checker_scatter.skip_pdf, "checkered_spheres diffuse scatter should not skip pdf");
    expect_true(checker_scatter.pdf != nullptr, "checkered_spheres diffuse scatter should provide pdf");
    expect_vec3_near(checker_scatter.attenuation, Vec3d {0.9, 0.9, 0.9}, 1e-12,
        "checkered_spheres checker attenuation should match odd color branch");

    const rt::scene::SceneIR bouncing_scene = rt::scene::build_scene("bouncing_spheres");
    expect_true(has_variant<rt::scene::MaterialDesc, rt::scene::MetalMaterial>(bouncing_scene.materials()),
        "bouncing_spheres should include metal material");
    expect_true(has_variant<rt::scene::MaterialDesc, rt::scene::DielectricMaterial>(bouncing_scene.materials()),
        "bouncing_spheres should include dielectric material");
    const rt::scene::CpuSceneAdapterResult adapted_bouncing = rt::scene::adapt_to_cpu(bouncing_scene);
    expect_true(adapted_bouncing.world.has_value(), "bouncing_spheres should adapt to world");
    const Ray glass_ray {Vec3d {0.0, 1.0, -4.0}, Vec3d {0.0, 0.0, 1.0}};
    HitRecord glass_hit;
    expect_true(adapted_bouncing.world->hit(glass_ray, Interval {0.001, infinity}, glass_hit),
        "bouncing_spheres ray should hit glass hero sphere");
    ScatterRecord glass_scatter;
    expect_true(glass_hit.mat->scatter(glass_ray, glass_hit, glass_scatter),
        "glass hero sphere should scatter");
    expect_true(glass_scatter.skip_pdf, "glass hero sphere scatter should be specular");
    expect_vec3_near(glass_scatter.attenuation, Vec3d {1.0, 1.0, 1.0}, 1e-12,
        "glass hero sphere attenuation should be white");

    const Ray metal_ray {Vec3d {4.0, 1.0, -4.0}, Vec3d {0.0, 0.0, 1.0}};
    HitRecord metal_hit;
    expect_true(adapted_bouncing.world->hit(metal_ray, Interval {0.001, infinity}, metal_hit),
        "bouncing_spheres ray should hit metal hero sphere");
    ScatterRecord metal_scatter;
    expect_true(metal_hit.mat->scatter(metal_ray, metal_hit, metal_scatter),
        "metal hero sphere should scatter");
    expect_true(metal_scatter.skip_pdf, "metal hero sphere scatter should be specular");
    expect_vec3_near(metal_scatter.attenuation, Vec3d {0.7, 0.6, 0.5}, 1e-12,
        "metal hero sphere attenuation should match configured albedo");
    expect_true((metal_scatter.attenuation - glass_scatter.attenuation).cwiseAbs().maxCoeff() > 0.1,
        "metal and glass attenuation should be behaviorally distinct");

    const rt::scene::SceneIR simple_light_scene = rt::scene::build_scene("simple_light");
    const rt::scene::CpuSceneAdapterResult adapted_simple_light = rt::scene::adapt_to_cpu(simple_light_scene);
    expect_true(adapted_simple_light.world.has_value(), "simple_light should adapt to world");
    expect_true(adapted_simple_light.lights.has_value(), "simple_light should produce non-empty lights");
    const Vec3d light_origin {0.0, 7.0, -8.0};
    const Vec3d light_direction {0.0, 0.0, 1.0};
    expect_true(adapted_simple_light.lights->pdf_value(light_origin, light_direction) > 0.0,
        "simple_light lights should be sampleable");
    const Ray light_ray {light_origin, light_direction};
    HitRecord light_hit;
    expect_true(adapted_simple_light.world->hit(light_ray, Interval {0.001, infinity}, light_hit),
        "simple_light ray should hit emissive geometry");
    const Vec3d light_emitted = light_hit.mat->emitted(light_ray, light_hit, light_hit.u, light_hit.v, light_hit.p);
    expect_true(light_emitted.maxCoeff() > 0.0, "simple_light emissive hit should emit positive radiance");

    rt::scene::SceneIR medium_scene;
    const int medium_texture = medium_scene.add_texture(
        rt::scene::ConstantColorTextureDesc {.color = Eigen::Vector3d {0.2, 0.4, 0.9}});
    const int medium_material =
        medium_scene.add_material(rt::scene::IsotropicVolumeMaterial {.albedo_texture = medium_texture});
    const int medium_boundary = medium_scene.add_shape(
        rt::scene::SphereShape {.center = Eigen::Vector3d {0.0, 0.0, 0.0}, .radius = 1.0});
    medium_scene.add_medium(rt::scene::MediumInstance {
        .shape_index = medium_boundary,
        .material_index = medium_material,
        .density = 1000.0,
    });

    const rt::scene::CpuSceneAdapterResult adapted_medium = rt::scene::adapt_to_cpu(medium_scene);
    expect_true(adapted_medium.world.has_value(), "dense isotropic medium fixture should adapt to world");
    const Ray medium_ray {Vec3d {0.0, 0.0, -3.0}, Vec3d {0.0, 0.0, 1.0}};
    HitRecord medium_hit;
    bool medium_hit_found = false;
    for (int attempt = 0; attempt < 8 && !medium_hit_found; ++attempt) {
        medium_hit_found = adapted_medium.world->hit(medium_ray, Interval {0.001, infinity}, medium_hit);
    }
    expect_true(medium_hit_found, "dense isotropic medium ray should hit volume");
    expect_true(medium_hit.t >= 2.0 && medium_hit.t <= 4.0,
        "dense isotropic medium hit should lie within boundary segment");
    ScatterRecord medium_scatter;
    expect_true(medium_hit.mat->scatter(medium_ray, medium_hit, medium_scatter),
        "dense isotropic medium phase function should scatter");
    expect_true(!medium_scatter.skip_pdf, "isotropic scatter should not skip pdf");
    expect_true(medium_scatter.pdf != nullptr, "isotropic scatter should provide pdf");
    expect_vec3_near(medium_scatter.attenuation, Vec3d {0.2, 0.4, 0.9}, 1e-12,
        "isotropic scatter attenuation should match configured albedo");
    expect_near(medium_hit.mat->scattering_pdf(medium_ray, medium_hit, Ray {medium_hit.p, Vec3d {1.0, 0.0, 0.0}}),
        1.0 / (4.0 * pi), 1e-12, "isotropic scattering pdf should be uniform over sphere");

    rt::scene::SceneIR transformed_emissive_scene;
    const int emissive_texture = transformed_emissive_scene.add_texture(
        rt::scene::ConstantColorTextureDesc {.color = Eigen::Vector3d {4.0, 4.0, 4.0}});
    const int emissive_material =
        transformed_emissive_scene.add_material(rt::scene::EmissiveMaterial {.emission_texture = emissive_texture});
    const int quad = transformed_emissive_scene.add_shape(rt::scene::QuadShape {
        .origin = Eigen::Vector3d {0.0, 0.0, 0.0},
        .edge_u = Eigen::Vector3d {1.0, 0.0, 0.0},
        .edge_v = Eigen::Vector3d {0.0, 1.0, 0.0},
    });
    rt::scene::Transform translated = rt::scene::Transform::identity();
    translated.translation = Eigen::Vector3d {0.0, 1.0, 0.0};
    transformed_emissive_scene.add_instance(rt::scene::SurfaceInstance {
        .shape_index = quad,
        .material_index = emissive_material,
        .transform = translated,
    });

    const rt::scene::CpuSceneAdapterResult adapted_transformed_emissive =
        rt::scene::adapt_to_cpu(transformed_emissive_scene);
    expect_true(adapted_transformed_emissive.world.has_value(), "transformed emissive should adapt to world");
    expect_true(!adapted_transformed_emissive.lights.has_value(),
        "transformed emissive should not be added to lights");

    return 0;
}
