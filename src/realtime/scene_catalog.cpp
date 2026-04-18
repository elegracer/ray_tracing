#include "realtime/scene_catalog.h"
#include "scene/shared_scene_builders.h"

namespace rt {

const std::vector<SceneCatalogEntry>& scene_catalog() {
    static const std::vector<SceneCatalogEntry> catalog = []() {
        std::vector<SceneCatalogEntry> out;
        out.reserve(scene::scene_metadata().size());
        for (const scene::SceneMetadata& metadata : scene::scene_metadata()) {
            out.push_back(SceneCatalogEntry {
                .id = metadata.id,
                .label = metadata.label,
                .supports_cpu_render = metadata.supports_cpu_render,
                .supports_realtime = metadata.supports_realtime,
            });
        }
        return out;
    }();
    return catalog;
}

const SceneCatalogEntry* find_scene_catalog_entry(std::string_view id) {
    for (const SceneCatalogEntry& entry : scene_catalog()) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

}  // namespace rt
