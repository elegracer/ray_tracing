#include "scene/cpu_scene_adapter.h"
#include "scene/shared_scene_builders.h"
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

    const rt::scene::SceneIR bouncing_scene = rt::scene::build_scene("bouncing_spheres");
    expect_true(has_variant<rt::scene::MaterialDesc, rt::scene::MetalMaterial>(bouncing_scene.materials()),
        "bouncing_spheres should include metal material");
    expect_true(has_variant<rt::scene::MaterialDesc, rt::scene::DielectricMaterial>(bouncing_scene.materials()),
        "bouncing_spheres should include dielectric material");
    const rt::scene::CpuSceneAdapterResult adapted_bouncing = rt::scene::adapt_to_cpu(bouncing_scene);
    expect_true(adapted_bouncing.world.has_value(), "bouncing_spheres should adapt to world");

    const rt::scene::SceneIR simple_light_scene = rt::scene::build_scene("simple_light");
    const rt::scene::CpuSceneAdapterResult adapted_simple_light = rt::scene::adapt_to_cpu(simple_light_scene);
    expect_true(adapted_simple_light.world.has_value(), "simple_light should adapt to world");
    expect_true(adapted_simple_light.lights.has_value(), "simple_light should produce non-empty lights");

    return 0;
}
