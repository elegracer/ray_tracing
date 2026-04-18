#include "scene/realtime_scene_adapter.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

#include <type_traits>

int main() {
    const rt::scene::SceneIR cornell_smoke = rt::scene::build_scene("cornell_smoke");
    const rt::SceneDescription adapted = rt::scene::adapt_to_realtime(cornell_smoke);
    const rt::PackedScene packed = adapted.pack();

    expect_true(!packed.materials.empty(), "realtime adapter preserves materials");
    expect_true(!packed.quads.empty(), "realtime adapter lowers room geometry");
    expect_true(packed.media.size() == 2, "cornell smoke keeps two media");
    expect_true(packed.medium_count == 2, "packed medium count");
    expect_true(std::holds_alternative<rt::IsotropicVolumeMaterial>(packed.materials.back()),
        "isotropic material preserved for media");

    const rt::HomogeneousMediumPrimitive& first_medium = packed.media.front();
    expect_true(first_medium.boundary_type == 1, "box media lower to packed box boundaries");
    expect_true(first_medium.material_index >= 0, "medium material index");
    expect_true(first_medium.density > 0.0f, "medium density kept positive");
    return 0;
}
