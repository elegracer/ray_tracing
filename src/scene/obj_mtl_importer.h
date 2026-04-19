#pragma once

#include "scene/shared_scene_ir.h"

#include <filesystem>
#include <string>
#include <vector>

namespace rt::scene {

struct ObjImportResult {
    SceneIR scene_ir {};
    std::vector<std::string> dependencies;
};

ObjImportResult import_obj_mtl(const std::filesystem::path& obj_file);

}  // namespace rt::scene
