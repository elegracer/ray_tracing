#pragma once

#include "scene/scene_ir_v2.h"

#include <filesystem>
#include <string>
#include <vector>

namespace rt::scene {

struct MaterialXOpenPbrDocument {
    std::filesystem::path source_path;
    std::string material_name;
    std::string document_color_space;
    SceneOpenPbrSurface surface;
};

MaterialXOpenPbrDocument load_materialx_openpbr(const std::filesystem::path& path);
std::vector<MaterialXOpenPbrDocument> load_materialx_openpbr_directory(
    const std::filesystem::path& directory);

} // namespace rt::scene
