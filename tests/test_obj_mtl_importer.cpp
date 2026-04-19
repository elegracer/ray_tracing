#include "scene/obj_mtl_importer.h"

#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace fs = std::filesystem;

namespace {

void write_text_file(const fs::path& path, std::string_view contents) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << contents;
}

std::string require_error(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::exception& ex) {
        return ex.what();
    }
    throw std::runtime_error("expected exception");
}

void test_obj_importer_creates_triangle_mesh_and_material() {
    const fs::path root = fs::temp_directory_path() / "obj_mtl_importer_basic";
    fs::remove_all(root);
    const fs::path obj_file = root / "triangle.obj";
    const fs::path mtl_file = root / "triangle.mtl";
    write_text_file(obj_file,
        "mtllib triangle.mtl\n"
        "usemtl matte\n"
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n");
    write_text_file(mtl_file, "newmtl matte\nKd 0.8 0.7 0.6\n");

    const rt::scene::ObjImportResult imported = rt::scene::import_obj_mtl(obj_file);
    expect_true(imported.scene_ir.shapes().size() == 1, "mesh shape count");
    expect_true(std::holds_alternative<rt::scene::TriangleMeshShape>(imported.scene_ir.shapes().front()), "mesh type");
    expect_true(imported.scene_ir.materials().size() == 1, "material count");
    expect_true(imported.scene_ir.surface_instances().size() == 1, "instance count");
    expect_true(imported.dependencies.size() == 2, "dependency count");
    expect_true(imported.dependencies[0] == obj_file.lexically_normal().string(), "obj dependency");
    expect_true(imported.dependencies[1] == mtl_file.lexically_normal().string(), "mtl dependency");
}

void test_obj_importer_rebases_map_kd_relative_to_mtl() {
    const fs::path root = fs::temp_directory_path() / "obj_mtl_importer_texture_rebase";
    fs::remove_all(root);
    const fs::path obj_file = root / "models" / "triangle.obj";
    const fs::path mtl_file = root / "materials" / "triangle.mtl";
    const fs::path texture_file = root / "materials" / "textures" / "albedo.png";
    write_text_file(obj_file,
        "mtllib ../materials/triangle.mtl\n"
        "usemtl matte\n"
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n");
    write_text_file(mtl_file, "newmtl matte\nmap_Kd textures/albedo.png\n");
    write_text_file(texture_file, "not-an-image");

    const rt::scene::ObjImportResult imported = rt::scene::import_obj_mtl(obj_file);
    expect_true(imported.scene_ir.textures().size() == 1, "texture count");
    expect_true(std::holds_alternative<rt::scene::ImageTextureDesc>(imported.scene_ir.textures().front()), "image texture");
    const auto& image = std::get<rt::scene::ImageTextureDesc>(imported.scene_ir.textures().front());
    expect_true(image.path == texture_file.lexically_normal().string(), "map_Kd rebased from mtl");
    expect_true(imported.dependencies.size() == 3, "dependency count with texture");
    expect_true(imported.dependencies[2] == texture_file.lexically_normal().string(), "texture dependency");
}

void test_obj_importer_errors_are_prefixed_with_obj_path() {
    const fs::path root = fs::temp_directory_path() / "obj_mtl_importer_missing_obj";
    fs::remove_all(root);
    const fs::path obj_file = root / "missing.obj";

    const std::string error = require_error([&]() { (void)rt::scene::import_obj_mtl(obj_file); });
    const std::string expected_prefix = obj_file.lexically_normal().string() + ": ";
    expect_true(error.rfind(expected_prefix, 0) == 0, "obj error prefix");
}

}  // namespace

int main() {
    test_obj_importer_creates_triangle_mesh_and_material();
    test_obj_importer_rebases_map_kd_relative_to_mtl();
    test_obj_importer_errors_are_prefixed_with_obj_path();
    return 0;
}
