#include "scene/scene_file_catalog.h"

#include "scene/shared_scene_builders.h"
#include "scene/yaml_scene_loader.h"

#include <algorithm>
#include <stdexcept>

namespace rt::scene {
namespace fs = std::filesystem;

namespace {

fs::path source_tree_root() {
    return fs::path(__FILE__).parent_path().parent_path().parent_path().lexically_normal();
}

fs::path resolve_scan_root(const fs::path& root) {
    if (root.is_absolute() || fs::exists(root)) {
        return root.lexically_normal();
    }
    const fs::path source_relative = source_tree_root() / root;
    if (fs::exists(source_relative)) {
        return source_relative.lexically_normal();
    }
    return root.lexically_normal();
}

std::vector<fs::path> collect_scene_files(const fs::path& root) {
    std::vector<fs::path> out;
    if (!fs::exists(root)) {
        return out;
    }

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().filename() == "scene.yaml") {
            out.push_back(entry.path().lexically_normal());
        }
    }

    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

SceneFileCatalog::SceneFileCatalog() {
    replace_records(builtin_records());
}

void SceneFileCatalog::scan_directory(const fs::path& root) {
    const fs::path resolved_root = resolve_scan_root(root);
    std::vector<CatalogRecord> next = builtin_records();
    for (const fs::path& scene_file : collect_scene_files(resolved_root)) {
        CatalogRecord loaded = make_record(load_scene_definition(scene_file), scene_file, false);
        auto existing = std::find_if(next.begin(), next.end(), [&](const CatalogRecord& record) {
            return record.definition.metadata.id == loaded.definition.metadata.id;
        });
        if (existing == next.end()) {
            next.push_back(std::move(loaded));
            continue;
        }
        if (!existing->is_builtin) {
            throw std::runtime_error("duplicate scene id in file catalog: " + loaded.definition.metadata.id);
        }
        *existing = std::move(loaded);
    }

    scanned_root_ = resolved_root;
    replace_records(std::move(next));
}

ReloadStatus SceneFileCatalog::reload_scene(std::string_view scene_id) {
    auto record = find_record(scene_id);
    if (record == records_.end()) {
        return ReloadStatus {.ok = false, .error_message = "unknown scene id"};
    }

    try {
        if (record->is_builtin) {
            const SceneDefinition* definition = find_builtin_scene_definition(scene_id);
            if (definition == nullptr) {
                throw std::runtime_error("missing builtin scene definition");
            }
            *record = make_record(*definition, {}, true);
        } else {
            SceneDefinition reloaded = load_scene_definition(record->scene_file);
            if (reloaded.metadata.id != scene_id) {
                throw std::runtime_error("reloaded scene id changed");
            }
            *record = make_record(std::move(reloaded), record->scene_file, false);
        }
        rebuild_entries();
        return ReloadStatus {.ok = true};
    } catch (const std::exception& ex) {
        return ReloadStatus {.ok = false, .error_message = ex.what()};
    }
}

const SceneDefinition* SceneFileCatalog::find_scene(std::string_view scene_id) const {
    const auto record = find_record(scene_id);
    return record != records_.end() ? &record->definition : nullptr;
}

const CpuRenderPreset* SceneFileCatalog::find_cpu_render_preset(
    std::string_view scene_id, std::string_view preset_id) const {
    const auto record = find_record(scene_id);
    if (record == records_.end()) {
        return nullptr;
    }
    for (const CpuRenderPreset& preset : record->cpu_presets) {
        if (preset.preset_id == preset_id) {
            return &preset;
        }
    }
    return nullptr;
}

const CpuRenderPreset* SceneFileCatalog::default_cpu_render_preset(std::string_view scene_id) const {
    return find_cpu_render_preset(scene_id, "default");
}

const RealtimeViewPreset* SceneFileCatalog::find_realtime_view_preset(std::string_view scene_id) const {
    const auto record = find_record(scene_id);
    if (record == records_.end() || !record->definition.realtime_preset.has_value()) {
        return nullptr;
    }
    return &*record->definition.realtime_preset;
}

const std::vector<SceneMetadata>& SceneFileCatalog::entries() const {
    return entries_;
}

std::uint64_t SceneFileCatalog::generation() const {
    return generation_;
}

SceneFileCatalog::CatalogRecord SceneFileCatalog::make_record(
    SceneDefinition definition, fs::path scene_file, bool is_builtin) {
    CatalogRecord record;
    record.definition = std::move(definition);
    record.scene_file = std::move(scene_file);
    record.is_builtin = is_builtin;
    return record;
}

void SceneFileCatalog::refresh_record_views(CatalogRecord& record) {
    record.metadata_view = SceneMetadata {
        .id = record.definition.metadata.id,
        .label = record.definition.metadata.label,
        .background = record.definition.metadata.background,
        .supports_cpu_render = record.definition.metadata.supports_cpu_render,
        .supports_realtime = record.definition.metadata.supports_realtime,
    };
    record.cpu_presets.clear();
    record.cpu_presets.reserve(record.definition.cpu_presets.size());
    for (const SceneDefinitionCpuRenderPreset& preset : record.definition.cpu_presets) {
        record.cpu_presets.push_back(CpuRenderPreset {
            .scene_id = preset.scene_id,
            .preset_id = preset.preset_id,
            .samples_per_pixel = preset.samples_per_pixel,
            .camera = preset.camera,
        });
    }
}

std::vector<SceneFileCatalog::CatalogRecord> SceneFileCatalog::builtin_records() {
    std::vector<CatalogRecord> records;
    records.reserve(builtin_scene_definitions().size());
    for (const SceneDefinition& definition : builtin_scene_definitions()) {
        records.push_back(make_record(definition, {}, true));
    }
    return records;
}

void SceneFileCatalog::replace_records(std::vector<CatalogRecord> records) {
    records_ = std::move(records);
    ++generation_;
    rebuild_entries();
}

void SceneFileCatalog::rebuild_entries() {
    entries_.clear();
    entries_.reserve(records_.size());
    for (CatalogRecord& record : records_) {
        refresh_record_views(record);
        entries_.push_back(record.metadata_view);
    }
}

std::vector<SceneFileCatalog::CatalogRecord>::iterator SceneFileCatalog::find_record(std::string_view scene_id) {
    return std::find_if(records_.begin(), records_.end(), [&](const CatalogRecord& record) {
        return record.definition.metadata.id == scene_id;
    });
}

std::vector<SceneFileCatalog::CatalogRecord>::const_iterator SceneFileCatalog::find_record(
    std::string_view scene_id) const {
    return std::find_if(records_.begin(), records_.end(), [&](const CatalogRecord& record) {
        return record.definition.metadata.id == scene_id;
    });
}

SceneFileCatalog& global_scene_file_catalog() {
    static SceneFileCatalog catalog = []() {
        SceneFileCatalog out;
        try {
            out.scan_directory("assets/scenes");
        } catch (...) {
            // Keep builtin fallback available even if file-backed scene loading fails at startup.
        }
        return out;
    }();
    return catalog;
}

}  // namespace rt::scene
