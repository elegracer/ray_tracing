#pragma once

#include <string_view>
#include <vector>

namespace rt {

struct SceneCatalogEntry {
    std::string_view id;
    std::string_view label;
    bool supports_cpu_render = false;
    bool supports_realtime = false;
};

const std::vector<SceneCatalogEntry>& scene_catalog();
const SceneCatalogEntry* find_scene_catalog_entry(std::string_view id);

}  // namespace rt
