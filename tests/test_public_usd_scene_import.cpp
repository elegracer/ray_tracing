#include "scene/openusd_stage_importer.h"
#include "scene/realtime_scene_adapter.h"

#include <cstdlib>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <variant>

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: test_public_usd_scene_import <stage.usd>\n";
        return EXIT_FAILURE;
    }
    if (!rt::scene::openusd_stage_importer_available()) {
        std::cerr << "public USD acceptance requires RT_ENABLE_OPENUSD=ON\n";
        return EXIT_FAILURE;
    }

    try {
        const rt::scene::SceneIRv2 scene =
            rt::scene::import_openusd_stage(std::filesystem::path {argv[1]});
        std::size_t meshes_with_primvars = 0;
        std::size_t meshes_with_material_subsets = 0;
        std::size_t connected_surface_materials = 0;
        std::size_t resolved_image_textures = 0;
        for (const rt::scene::ScenePrim& prim : scene.prims()) {
            if (prim.geometry
                && std::holds_alternative<rt::scene::SceneMeshGeometry>(*prim.geometry)) {
                const auto& mesh = std::get<rt::scene::SceneMeshGeometry>(*prim.geometry);
                meshes_with_primvars += !mesh.primvars.empty();
                meshes_with_material_subsets += !mesh.material_subsets.empty();
            }
            if (prim.material
                && std::holds_alternative<rt::scene::SceneOpenPbrSurface>(*prim.material)) {
                const auto& material = std::get<rt::scene::SceneOpenPbrSurface>(*prim.material);
                connected_surface_materials += !material.connections.empty();
            }
            if (prim.texture && prim.texture->node == rt::scene::SceneTextureNode::image
                && prim.asset_references.size() == 1
                && !prim.asset_references.front().resolved_path.empty()) {
                ++resolved_image_textures;
            }
        }
        if (meshes_with_primvars == 0 || meshes_with_material_subsets == 0
            || connected_surface_materials == 0 || resolved_image_textures == 0) {
            throw std::runtime_error(
                "public stage did not compile mesh primvars, material subsets, connected "
                "PreviewSurface materials, and resolved image textures");
        }
        const rt::PackedScene realtime = rt::scene::adapt_scene_ir_v2_to_realtime(scene).pack();
        if (realtime.triangle_count == 0 || realtime.material_count == 0
            || realtime.texture_count == 0) {
            throw std::runtime_error(
                "public stage did not compile into realtime triangles, materials, and textures");
        }
        std::cout << "imported public USD acceptance stage with " << scene.prims().size()
                  << " SceneIR v2 prims; meshes_with_primvars=" << meshes_with_primvars
                  << ", meshes_with_material_subsets=" << meshes_with_material_subsets
                  << ", connected_surface_materials=" << connected_surface_materials
                  << ", resolved_image_textures=" << resolved_image_textures
                  << ", realtime_triangles=" << realtime.triangle_count
                  << ", realtime_materials=" << realtime.material_count
                  << ", realtime_textures=" << realtime.texture_count << '\n';
    } catch (const std::exception& error) {
        std::cerr << "public USD acceptance import failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
