#include "scene/obj_mtl_importer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <Eigen/Core>

#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rt::scene {
namespace {

using DependencySet = std::unordered_set<std::string>;

void append_dependency(std::vector<std::string>& dependencies, DependencySet& seen, const std::filesystem::path& path) {
    const std::string normalized = path.lexically_normal().string();
    if (seen.insert(normalized).second) {
        dependencies.push_back(normalized);
    }
}

std::vector<std::filesystem::path> referenced_mtl_files(const std::filesystem::path& obj_file) {
    std::ifstream stream(obj_file);
    if (!stream) {
        throw std::runtime_error("failed to open obj file");
    }

    std::vector<std::filesystem::path> result;
    std::unordered_set<std::string> seen;
    std::string keyword;
    while (stream >> keyword) {
        if (keyword == "mtllib") {
            std::string mtl_name;
            std::getline(stream >> std::ws, mtl_name);
            if (!mtl_name.empty()) {
                const std::filesystem::path mtl_path = (obj_file.parent_path() / mtl_name).lexically_normal();
                if (seen.insert(mtl_path.string()).second) {
                    result.push_back(mtl_path);
                }
            }
        } else {
            std::string ignored;
            std::getline(stream, ignored);
        }
    }
    return result;
}

std::unordered_map<std::string, std::filesystem::path> material_roots_by_name(
    const std::vector<std::filesystem::path>& mtl_files) {
    std::unordered_map<std::string, std::filesystem::path> roots;
    for (const std::filesystem::path& mtl_file : mtl_files) {
        std::ifstream stream(mtl_file);
        if (!stream) {
            throw std::runtime_error(mtl_file.string() + ": failed to open mtl file");
        }

        std::string keyword;
        while (stream >> keyword) {
            if (keyword == "newmtl") {
                std::string material_name;
                std::getline(stream >> std::ws, material_name);
                if (!material_name.empty()) {
                    roots.emplace(material_name, mtl_file.parent_path());
                }
            } else {
                std::string ignored;
                std::getline(stream, ignored);
            }
        }
    }
    return roots;
}

int add_diffuse_material(SceneIR& scene_ir, const tinyobj::material_t* material,
    const std::unordered_map<std::string, std::filesystem::path>& material_roots, const std::filesystem::path& obj_root,
    std::vector<std::string>& dependencies, DependencySet& dependency_set) {
    if (material != nullptr && !material->diffuse_texname.empty()) {
        std::filesystem::path texture_path = material->diffuse_texname;
        if (texture_path.is_relative()) {
            const auto root_it = material_roots.find(material->name);
            const std::filesystem::path& texture_root =
                root_it != material_roots.end() ? root_it->second : obj_root;
            texture_path = texture_root / texture_path;
        }
        texture_path = texture_path.lexically_normal();
        append_dependency(dependencies, dependency_set, texture_path);
        const int texture = scene_ir.add_texture(ImageTextureDesc {.path = texture_path.string()});
        return scene_ir.add_material(DiffuseMaterial {.albedo_texture = texture});
    }

    Eigen::Vector3d color = Eigen::Vector3d::Ones();
    if (material != nullptr) {
        color = Eigen::Vector3d {material->diffuse[0], material->diffuse[1], material->diffuse[2]};
    }
    const int texture = scene_ir.add_texture(ConstantColorTextureDesc {.color = color});
    return scene_ir.add_material(DiffuseMaterial {.albedo_texture = texture});
}

}  // namespace

ObjImportResult import_obj_mtl(const std::filesystem::path& obj_file) {
    const std::filesystem::path normalized_obj = obj_file.lexically_normal();
    try {
        tinyobj::ObjReaderConfig config;
        config.triangulate = true;
        config.mtl_search_path = normalized_obj.parent_path().string();

        tinyobj::ObjReader reader;
        if (!reader.ParseFromFile(normalized_obj.string(), config)) {
            throw std::runtime_error(reader.Error().empty() ? "failed to parse obj file" : reader.Error());
        }

        ObjImportResult out;
        DependencySet dependency_set;
        append_dependency(out.dependencies, dependency_set, normalized_obj);
        const std::vector<std::filesystem::path> mtl_files = referenced_mtl_files(normalized_obj);
        for (const std::filesystem::path& mtl_file : mtl_files) {
            append_dependency(out.dependencies, dependency_set, mtl_file);
        }
        const auto material_roots = material_roots_by_name(mtl_files);

        const tinyobj::attrib_t& attrib = reader.GetAttrib();
        TriangleMeshShape base_mesh;
        base_mesh.positions.reserve(attrib.vertices.size() / 3);
        for (std::size_t i = 0; i + 2 < attrib.vertices.size(); i += 3) {
            base_mesh.positions.push_back(Eigen::Vector3d {
                static_cast<double>(attrib.vertices[i + 0]),
                static_cast<double>(attrib.vertices[i + 1]),
                static_cast<double>(attrib.vertices[i + 2]),
            });
        }

        std::unordered_map<int, std::vector<Eigen::Vector3i>> triangles_by_material;
        for (const tinyobj::shape_t& shape : reader.GetShapes()) {
            std::size_t index_offset = 0;
            for (std::size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
                const int vertex_count = shape.mesh.num_face_vertices[face];
                if (vertex_count != 3) {
                    throw std::runtime_error("only triangulated faces are supported");
                }

                Eigen::Vector3i triangle;
                for (int corner = 0; corner < 3; ++corner) {
                    const tinyobj::index_t index = shape.mesh.indices[index_offset + static_cast<std::size_t>(corner)];
                    if (index.vertex_index < 0) {
                        throw std::runtime_error("negative obj vertex indices are not supported");
                    }
                    triangle[corner] = index.vertex_index;
                }
                index_offset += 3;

                const int material_index =
                    face < shape.mesh.material_ids.size() ? shape.mesh.material_ids[face] : -1;
                triangles_by_material[material_index].push_back(triangle);
            }
        }

        const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();
        for (const auto& [material_index, triangles] : triangles_by_material) {
            TriangleMeshShape mesh = base_mesh;
            mesh.triangles = triangles;

            const tinyobj::material_t* material = nullptr;
            if (material_index >= 0 && static_cast<std::size_t>(material_index) < materials.size()) {
                material = &materials[static_cast<std::size_t>(material_index)];
            }

            const int shape_index = out.scene_ir.add_shape(mesh);
            const int scene_material = add_diffuse_material(
                out.scene_ir, material, material_roots, normalized_obj.parent_path(), out.dependencies, dependency_set);
            out.scene_ir.add_instance(SurfaceInstance {
                .shape_index = shape_index,
                .material_index = scene_material,
            });
        }

        return out;
    } catch (const std::exception& ex) {
        throw std::runtime_error(normalized_obj.string() + ": " + ex.what());
    }
}

}  // namespace rt::scene
