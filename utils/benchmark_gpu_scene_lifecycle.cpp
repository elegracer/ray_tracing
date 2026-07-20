#include "realtime/gpu/optix_renderer.h"
#include "realtime/scene_description.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kPrimitiveCount = 1024;
constexpr int kIterations = 20;

rt::PackedScene make_instanced_scene(int primitive_count, double position_offset, double albedo) {
    rt::SceneDescription scene;
    const int material =
        scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {albedo, 0.3, 0.2}});
    for (int i = 0; i < primitive_count; ++i) {
        const int x = i % 32;
        const int y = i / 32;
        scene.add_sphere(rt::SpherePrimitive {
            .material_index = material,
            .center = Eigen::Vector3d {static_cast<double>(x) * 0.3 + position_offset,
                static_cast<double>(y) * 0.3, -4.0},
            .radius = 0.1,
            .dynamic = i == 0,
            .acceleration_prototype_id = 1,
            .acceleration_instance_id = i,
        });
    }
    return scene.pack();
}

struct TimingSummary {
    double avg_ms = 0.0;
    double p95_ms = 0.0;
    double max_ms = 0.0;
};

TimingSummary summarize(std::vector<double> values) {
    if (values.empty()) {
        throw std::runtime_error("GPU lifecycle timing set is empty");
    }
    std::sort(values.begin(), values.end());
    double sum = 0.0;
    for (double value : values) {
        if (!std::isfinite(value) || value < 0.0) {
            throw std::runtime_error("GPU lifecycle timing is invalid");
        }
        sum += value;
    }
    const std::size_t p95_index =
        std::min(values.size() - 1, static_cast<std::size_t>(std::ceil(values.size() * 0.95) - 1));
    return TimingSummary {
        .avg_ms = sum / static_cast<double>(values.size()),
        .p95_ms = values[p95_index],
        .max_ms = values.back(),
    };
}

void require_kind(const rt::AccelerationUpdateStats& stats, rt::AccelerationUpdateKind expected,
    std::string_view label) {
    if (stats.kind != expected) {
        throw std::runtime_error(std::string(label) + " expected "
                                 + std::string(rt::acceleration_update_kind_name(expected))
                                 + " but observed "
                                 + std::string(rt::acceleration_update_kind_name(stats.kind)));
    }
}

void write_summary(std::ofstream& output, std::string_view name, const TimingSummary& summary,
    bool trailing_comma) {
    output << "    \"" << name << "\": {\"avg_ms\": " << summary.avg_ms
           << ", \"p95_ms\": " << summary.p95_ms << ", \"max_ms\": " << summary.max_ms << "}"
           << (trailing_comma ? "," : "") << "\n";
}

} // namespace

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        throw std::invalid_argument("usage: benchmark_gpu_scene_lifecycle OUTPUT_JSON");
    }

    const rt::PackedScene base = make_instanced_scene(kPrimitiveCount, 0.0, 0.7);
    const rt::PackedScene material_update = make_instanced_scene(kPrimitiveCount, 0.0, 0.9);
    const rt::PackedScene moved = make_instanced_scene(kPrimitiveCount, 0.01, 0.9);
    const rt::PackedScene topology_change = make_instanced_scene(kPrimitiveCount + 1, 0.0, 0.9);

    rt::SharedGpuSceneState state;
    std::vector<double> rebuild_ms;
    std::vector<double> update_ms;
    std::vector<double> refit_ms;
    std::vector<double> reuse_ms;
    rt::AccelerationUpdateStats representative_rebuild {};

    // Warm device allocation, driver state, and every lifecycle path before measurement.
    require_kind(state.prepare(base), rt::AccelerationUpdateKind::rebuild, "warmup rebuild");
    require_kind(state.prepare(base), rt::AccelerationUpdateKind::reuse, "warmup reuse");
    require_kind(state.prepare(material_update), rt::AccelerationUpdateKind::update,
        "warmup update");
    require_kind(state.prepare(moved), rt::AccelerationUpdateKind::refit, "warmup refit");
    require_kind(state.prepare(topology_change), rt::AccelerationUpdateKind::rebuild,
        "warmup topology rebuild");

    for (int iteration = 0; iteration < kIterations; ++iteration) {
        const rt::AccelerationUpdateStats rebuild = state.prepare(base);
        require_kind(rebuild, rt::AccelerationUpdateKind::rebuild, "base rebuild");
        rebuild_ms.push_back(rebuild.elapsed_ms);
        representative_rebuild = rebuild;

        const rt::AccelerationUpdateStats reuse = state.prepare(base);
        require_kind(reuse, rt::AccelerationUpdateKind::reuse, "identical reuse");
        reuse_ms.push_back(reuse.elapsed_ms);

        const rt::AccelerationUpdateStats update = state.prepare(material_update);
        require_kind(update, rt::AccelerationUpdateKind::update, "material update");
        update_ms.push_back(update.elapsed_ms);

        const rt::AccelerationUpdateStats refit = state.prepare(moved);
        require_kind(refit, rt::AccelerationUpdateKind::refit, "geometry refit");
        refit_ms.push_back(refit.elapsed_ms);

        const rt::AccelerationUpdateStats topology_rebuild = state.prepare(topology_change);
        require_kind(topology_rebuild, rt::AccelerationUpdateKind::rebuild, "topology rebuild");
        rebuild_ms.push_back(topology_rebuild.elapsed_ms);
    }

    if (representative_rebuild.prototype_count != 1
        || representative_rebuild.instance_count != kPrimitiveCount
        || representative_rebuild.instanced_primitive_count != kPrimitiveCount) {
        throw std::runtime_error("instancing diagnostics did not preserve prototype reuse");
    }

    const std::filesystem::path output_path = argv[1];
    std::filesystem::create_directories(output_path.parent_path());
    std::ofstream output(output_path);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open GPU lifecycle benchmark output");
    }
    output << std::fixed << std::setprecision(6);
    output << "{\n";
    output << "  \"schema_version\": 1,\n";
    output << "  \"iterations\": " << kIterations << ",\n";
    output << "  \"primitive_count\": " << kPrimitiveCount << ",\n";
    output << "  \"acceleration\": {\n";
    output << "    \"node_count\": " << representative_rebuild.node_count << ",\n";
    output << "    \"primitive_reference_count\": "
           << representative_rebuild.primitive_reference_count << ",\n";
    output << "    \"prototype_count\": " << representative_rebuild.prototype_count << ",\n";
    output << "    \"instance_count\": " << representative_rebuild.instance_count << ",\n";
    output << "    \"instanced_primitive_count\": "
           << representative_rebuild.instanced_primitive_count << "\n";
    output << "  },\n";
    output << "  \"timings\": {\n";
    write_summary(output, "rebuild", summarize(rebuild_ms), true);
    write_summary(output, "update", summarize(update_ms), true);
    write_summary(output, "refit", summarize(refit_ms), true);
    write_summary(output, "reuse", summarize(reuse_ms), false);
    output << "  }\n";
    output << "}\n";
    if (!output.good()) {
        throw std::runtime_error("failed to write GPU lifecycle benchmark output");
    }
    return EXIT_SUCCESS;
}
