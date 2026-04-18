#include "realtime/scene_catalog.h"

namespace rt {
namespace {

const std::vector<SceneCatalogEntry> kSceneCatalog {
    {"bouncing_spheres", "Bouncing Spheres", true, false},
    {"checkered_spheres", "Checkered Spheres", true, false},
    {"earth_sphere", "Earth Sphere", true, false},
    {"perlin_spheres", "Perlin Spheres", true, false},
    {"quads", "Quads", true, false},
    {"simple_light", "Simple Light", true, false},
    {"cornell_smoke", "Cornell Smoke", true, false},
    {"cornell_smoke_extreme", "Cornell Smoke Extreme", true, false},
    {"cornell_box", "Cornell Box", true, false},
    {"cornell_box_extreme", "Cornell Box Extreme", true, false},
    {"cornell_box_and_sphere", "Cornell Box And Sphere", true, false},
    {"cornell_box_and_sphere_extreme", "Cornell Box And Sphere Extreme", true, false},
    {"rttnw_final_scene", "RTTNW Final Scene", true, false},
    {"rttnw_final_scene_extreme", "RTTNW Final Scene Extreme", true, false},
    {"smoke", "Realtime Smoke", false, true},
    {"final_room", "Final Room", false, true},
};

}  // namespace

const std::vector<SceneCatalogEntry>& scene_catalog() {
    return kSceneCatalog;
}

const SceneCatalogEntry* find_scene_catalog_entry(std::string_view id) {
    for (const SceneCatalogEntry& entry : kSceneCatalog) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

}  // namespace rt
