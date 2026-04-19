#include "scene/scene_file_catalog.h"
#include "realtime/scene_catalog.h"
#include "scene/shared_scene_builders.h"

#include <limits>

namespace rt {

const std::vector<SceneCatalogEntry>& scene_catalog() {
    static std::vector<SceneCatalogEntry> catalog;
    static std::uint64_t catalog_generation = std::numeric_limits<std::uint64_t>::max();
    const scene::SceneFileCatalog& source = scene::global_scene_file_catalog();
    if (catalog_generation != source.generation()) {
        catalog.clear();
        catalog.reserve(source.entries().size());
        for (const scene::SceneMetadata& metadata : source.entries()) {
            catalog.push_back(SceneCatalogEntry {
                .id = metadata.id,
                .label = metadata.label,
                .supports_cpu_render = metadata.supports_cpu_render,
                .supports_realtime = metadata.supports_realtime,
            });
        }
        catalog_generation = source.generation();
    }
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
