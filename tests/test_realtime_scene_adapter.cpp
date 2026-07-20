#include "scene/realtime_scene_adapter.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

#include <Eigen/Geometry>

#include <type_traits>

int main() {
    rt::scene::SceneIR triangle_scene;
    const int white = triangle_scene.add_texture(
        rt::scene::ConstantColorTextureDesc {.color = Eigen::Vector3d::Ones()});
    const int matte =
        triangle_scene.add_material(rt::scene::DiffuseMaterial {.albedo_texture = white});
    const int mesh = triangle_scene.add_shape(rt::scene::TriangleMeshShape {
        .positions =
            {
                Eigen::Vector3d {0.0, 0.0, 0.0},
                Eigen::Vector3d {1.0, 0.0, 0.0},
                Eigen::Vector3d {0.0, 1.0, 0.0},
            },
        .triangles = {Eigen::Vector3i {0, 1, 2}},
    });
    triangle_scene.add_instance(
        rt::scene::SurfaceInstance {.shape_index = mesh, .material_index = matte});

    const rt::SceneDescription adapted_triangle = rt::scene::adapt_to_realtime(triangle_scene);
    expect_true(adapted_triangle.triangles().size() == 1,
        "realtime adapter emits triangle primitive");

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

    rt::scene::SceneIRv2 scene_v2;
    scene_v2.add_prim(rt::scene::ScenePrim {.path = "/World"});
    scene_v2.add_prim(rt::scene::ScenePrim {
        .path = "/World/BaseTexture",
        .kind = rt::scene::ScenePrimKind::texture,
        .texture =
            rt::scene::SceneTexture {
                .node = rt::scene::SceneTextureNode::constant_color,
                .value = Eigen::Vector3d {0.2, 0.4, 0.8},
            },
    });
    rt::scene::SceneOpenPbrSurface blue;
    blue.connections.push_back({"base_color", rt::scene::SceneMaterialValueType::color3,
        "/World/BaseTexture", rt::scene::SceneTextureChannel::rgb});
    scene_v2.add_prim(rt::scene::ScenePrim {
        .path = "/World/Blue",
        .kind = rt::scene::ScenePrimKind::material,
        .material = rt::scene::SceneMaterial {blue},
    });
    rt::scene::SceneOpenPbrSurface metal;
    metal.base_metalness = 1.0;
    scene_v2.add_prim(rt::scene::ScenePrim {
        .path = "/World/Metal",
        .kind = rt::scene::ScenePrimKind::material,
        .material = rt::scene::SceneMaterial {metal},
    });
    scene_v2.add_prim(rt::scene::ScenePrim {.path = "/World/Prototypes"});
    scene_v2.add_prim(
        rt::scene::ScenePrim {
            .path = "/World/Prototypes/Panel",
            .kind = rt::scene::ScenePrimKind::geometry_prototype,
            .geometry =
                rt::scene::SceneMeshGeometry {
                    .points = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}},
                    .face_vertex_counts = {3, 3},
                    .face_vertex_indices = {0, 1, 2, 0, 2, 3},
                    .primvars =
                        {
                            rt::scene::ScenePrimvar {
                                .name = "normals",
                                .interpolation = rt::scene::ScenePrimvarInterpolation::face_varying,
                                .role = rt::scene::ScenePrimvarRole::normal,
                                .values = std::vector<Eigen::Vector3f>(6, {0.0f, 0.0f, 1.0f}),
                            },
                            rt::scene::ScenePrimvar {
                                .name = "st",
                                .interpolation = rt::scene::ScenePrimvarInterpolation::face_varying,
                                .role = rt::scene::ScenePrimvarRole::texcoord,
                                .values =
                                    std::vector<Eigen::Vector2f> {{0.0f, 0.0f}, {1.0f, 0.0f},
                                        {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}},
                            },
                        },
                    .material_subset_family_type =
                        rt::scene::SceneMaterialSubsetFamilyType::partition,
                    .material_subsets =
                        {
                            {.name = "blue", .face_indices = {0}, .material_path = "/World/Blue"},
                            {.name = "metal", .face_indices = {1}, .material_path = "/World/Metal"},
                        },
                },
        });
    Eigen::Matrix4d translated = Eigen::Matrix4d::Identity();
    translated(0, 3) = 2.0;
    scene_v2.add_prim(rt::scene::ScenePrim {
        .path = "/World/Panel",
        .kind = rt::scene::ScenePrimKind::surface,
        .local_to_parent = translated,
        .prototype_path = "/World/Prototypes/Panel",
        .material_path = "/World/Blue",
    });

    const rt::PackedScene packed_v2 = rt::scene::adapt_scene_ir_v2_to_realtime(scene_v2).pack();
    expect_true(packed_v2.texture_count == 1, "native v2 texture table");
    expect_true(packed_v2.material_count == 2, "native v2 material table");
    expect_true(packed_v2.triangle_count == 2, "native v2 mesh triangulation");
    expect_true(packed_v2.triangles[0].material_index == 0, "native v2 fallback subset material");
    expect_true(packed_v2.triangles[1].material_index == 1, "native v2 second subset material");
    expect_near(packed_v2.triangles[0].p0.x(), 2.0, 1e-9, "native v2 world transform");
    expect_true(packed_v2.triangles[0].has_vertex_normals, "native v2 vertex normals");
    expect_true(packed_v2.triangles[0].has_texcoords, "native v2 texcoords");
    expect_near(packed_v2.triangles[0].uv2.y(), 1.0, 1e-9, "native v2 face-varying uv");
    const auto& blue_material = std::get<rt::OpenPbrMaterialDesc>(packed_v2.materials[0]);
    expect_true(blue_material.compiled.color_textures.base_color.texture_index == 0,
        "native v2 connected OpenPBR texture binding");
    return 0;
}
