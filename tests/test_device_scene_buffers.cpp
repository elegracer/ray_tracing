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
    expect_true(view.sphere_count == 1, "sphere count");
    expect_true(view.quad_count == 2, "quad count");
    expect_true(view.triangle_count == 3, "triangle count");
    expect_true(view.medium_count == 4, "medium count");
    expect_true(view.texture_count == 5, "texture count");
    expect_true(view.image_texel_count == 6, "image texel count");
    expect_true(view.material_count == 7, "material count");

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
    expect_true(empty_view.sphere_count == 0, "empty sphere count");
    expect_true(empty_view.image_texel_count == 0, "empty image texel count");

    return 0;
}
