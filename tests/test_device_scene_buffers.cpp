#include "realtime/gpu/device_scene_buffers.h"
#include "test_support.h"

int main() {
    rt::GpuPreparedScene prepared;
    prepared.spheres.resize(1);
    prepared.quads.resize(2);
    prepared.triangles.resize(3);
    prepared.media.resize(4);
    prepared.textures.resize(5);
    prepared.image_texels.resize(6);
    prepared.materials.resize(7);
    prepared.openpbr_materials.resize(1);
    prepared.lights.resize(2);
    prepared.analytic_lights.resize(3);

    rt::DeviceSceneBuffers buffers;
    buffers.upload(prepared);
    const rt::DeviceSceneView view = buffers.view();

    expect_true(view.spheres != nullptr, "spheres uploaded");
    expect_true(view.quads != nullptr, "quads uploaded");
    expect_true(view.triangles != nullptr, "triangles uploaded");
    expect_true(view.media != nullptr, "media uploaded");
    expect_true(view.textures != nullptr, "textures uploaded");
    expect_true(view.image_texels != nullptr, "image texels uploaded");
    expect_true(view.materials != nullptr, "materials uploaded");
    expect_true(view.openpbr_materials != nullptr, "OpenPBR materials uploaded");
    expect_true(view.lights != nullptr, "lights uploaded");
    expect_true(view.analytic_lights != nullptr, "analytic lights uploaded");
    expect_true(view.sphere_count == 1, "sphere count");
    expect_true(view.quad_count == 2, "quad count");
    expect_true(view.triangle_count == 3, "triangle count");
    expect_true(view.medium_count == 4, "medium count");
    expect_true(view.texture_count == 5, "texture count");
    expect_true(view.image_texel_count == 6, "image texel count");
    expect_true(view.material_count == 7, "material count");
    expect_true(view.openpbr_material_count == 1, "OpenPBR material count");
    expect_true(view.light_count == 2, "light count");
    expect_true(view.analytic_light_count == 3, "analytic light count");

    rt::GpuPreparedScene empty;
    buffers.upload(empty);
    const rt::DeviceSceneView empty_view = buffers.view();
    expect_true(empty_view.spheres == nullptr, "empty spheres cleared");
    expect_true(empty_view.quads == nullptr, "empty quads cleared");
    expect_true(empty_view.triangles == nullptr, "empty triangles cleared");
    expect_true(empty_view.media == nullptr, "empty media cleared");
    expect_true(empty_view.textures == nullptr, "empty textures cleared");
    expect_true(empty_view.image_texels == nullptr, "empty image texels cleared");
    expect_true(empty_view.materials == nullptr, "empty materials cleared");
    expect_true(empty_view.openpbr_materials == nullptr, "empty OpenPBR materials cleared");
    expect_true(empty_view.sphere_count == 0, "empty sphere count");
    expect_true(empty_view.image_texel_count == 0, "empty image texel count");
    expect_true(empty_view.lights == nullptr, "empty lights cleared");
    expect_true(empty_view.light_count == 0, "empty light count");
    expect_true(empty_view.analytic_lights == nullptr, "empty analytic lights cleared");
    expect_true(empty_view.analytic_light_count == 0, "empty analytic light count");

    return 0;
}
