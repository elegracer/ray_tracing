#pragma once

#include "scene/scene_definition.h"

#include <filesystem>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rt::scene {

struct ReloadStatus {
    bool ok = false;
    std::string error_message;
};

class SceneFileCatalog {
   public:
    SceneFileCatalog();

    void scan_directory(const std::filesystem::path& root);
    ReloadStatus reload_scene(std::string_view scene_id);

    const SceneDefinition* find_scene(std::string_view scene_id) const;
    const CpuRenderPreset* find_cpu_render_preset(std::string_view scene_id, std::string_view preset_id) const;
    const CpuRenderPreset* default_cpu_render_preset(std::string_view scene_id) const;
    const RealtimeViewPreset* find_realtime_view_preset(std::string_view scene_id) const;
    const std::vector<SceneMetadata>& entries() const;
    std::uint64_t generation() const;

   private:
    struct CatalogRecord {
        SceneDefinition definition {};
        std::filesystem::path scene_file;
        bool is_builtin = false;
        std::vector<CpuRenderPreset> cpu_presets;
        SceneMetadata metadata_view {};
    };

    std::filesystem::path scanned_root_;
    std::vector<CatalogRecord> records_;
    std::vector<SceneMetadata> entries_;
    std::uint64_t generation_ = 0;

    static CatalogRecord make_record(SceneDefinition definition, std::filesystem::path scene_file, bool is_builtin);
    static void refresh_record_views(CatalogRecord& record);
    static std::vector<CatalogRecord> builtin_records();
    void replace_records(std::vector<CatalogRecord> records);
    void rebuild_entries();
    std::vector<CatalogRecord>::iterator find_record(std::string_view scene_id);
    std::vector<CatalogRecord>::const_iterator find_record(std::string_view scene_id) const;
};

SceneFileCatalog& global_scene_file_catalog();

}  // namespace rt::scene
