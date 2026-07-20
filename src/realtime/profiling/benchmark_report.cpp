#include "realtime/profiling/benchmark_report.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace rt::profiling {
namespace {

AggregateStats compute_stats(std::vector<double> values) {
    AggregateStats stats {};
    if (values.empty()) {
        return stats;
    }

    std::sort(values.begin(), values.end());

    const auto percentile_index = [&](double percentile) -> std::size_t {
        const double rank = std::ceil(percentile * static_cast<double>(values.size()));
        if (rank <= 1.0) {
            return 0;
        }
        const std::size_t idx = static_cast<std::size_t>(rank - 1.0);
        return std::min(idx, values.size() - 1);
    };

    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }

    stats.avg = sum / static_cast<double>(values.size());
    stats.p50 = values[percentile_index(0.50)];
    stats.p95 = values[percentile_index(0.95)];
    stats.p99 = values[percentile_index(0.99)];
    stats.max = values.back();
    return stats;
}

const char* bool_text(bool value) {
    return value ? "true" : "false";
}

std::string escape_csv_field(const std::string& value) {
    if (value.find_first_of(",\"\n\r") == std::string::npos) {
        return value;
    }

    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('"');
    return escaped;
}

std::string escape_json_string(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    const char* hex = "0123456789abcdef";

    for (unsigned char ch : value) {
        switch (ch) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (ch < 0x20) {
                    escaped += "\\u00";
                    escaped.push_back(hex[ch >> 4]);
                    escaped.push_back(hex[ch & 0x0F]);
                } else {
                    escaped.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    return escaped;
}

std::ofstream open_output_or_throw(const std::filesystem::path& path, const char* format_name) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error(
            "failed to open " + std::string(format_name) + " output file: " + path.string());
    }
    return out;
}

void ensure_write_ok_or_throw(const std::ofstream& out, const std::filesystem::path& path,
    const char* format_name) {
    if (!out.good()) {
        throw std::runtime_error(
            "failed to write " + std::string(format_name) + " output file: " + path.string());
    }
}

void write_aggregate_stats(std::ofstream& out, const char* name, const AggregateStats& stats,
    bool trailing_comma) {
    out << "    \"" << name << "\": {\"avg\": " << stats.avg << ", \"p50\": " << stats.p50
        << ", \"p95\": " << stats.p95 << ", \"p99\": " << stats.p99 << ", \"max\": " << stats.max
        << "}" << (trailing_comma ? "," : "") << "\n";
}

std::string fnv1a64_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open benchmark artifact: " + path.string());
    }

    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    std::uint64_t hash = kOffsetBasis;
    std::array<char, 8192> buffer {};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
            hash *= kPrime;
        }
    }
    if (!input.eof()) {
        throw std::runtime_error("failed to read benchmark artifact: " + path.string());
    }

    std::ostringstream digest;
    digest << std::hex << std::setfill('0') << std::setw(16) << hash;
    return digest.str();
}

const char* artifact_kind(const std::filesystem::path& path) {
    return path.extension() == ".csv" ? "raw_frame_samples" : "summary_and_provenance";
}

} // namespace

RunAggregate compute_aggregate(const std::vector<FrameStageSample>& frames) {
    std::vector<double> frame_ms;
    std::vector<double> pipeline_ms;
    std::vector<double> render_ms;
    std::vector<double> denoise_ms;
    std::vector<double> download_ms;
    std::vector<double> render_work_ms;
    std::vector<double> denoise_work_ms;
    std::vector<double> download_work_ms;
    std::vector<double> image_write_ms;
    std::vector<double> host_overhead_ms;

    frame_ms.reserve(frames.size());
    pipeline_ms.reserve(frames.size());
    render_ms.reserve(frames.size());
    denoise_ms.reserve(frames.size());
    download_ms.reserve(frames.size());
    render_work_ms.reserve(frames.size());
    denoise_work_ms.reserve(frames.size());
    download_work_ms.reserve(frames.size());
    image_write_ms.reserve(frames.size());
    host_overhead_ms.reserve(frames.size());

    for (const FrameStageSample& frame : frames) {
        frame_ms.push_back(frame.frame_ms);
        pipeline_ms.push_back(frame.pipeline_ms);
        render_ms.push_back(frame.render_ms);
        denoise_ms.push_back(frame.denoise_ms);
        download_ms.push_back(frame.download_ms);
        render_work_ms.push_back(frame.render_work_ms);
        denoise_work_ms.push_back(frame.denoise_work_ms);
        download_work_ms.push_back(frame.download_work_ms);
        image_write_ms.push_back(frame.image_write_ms);
        host_overhead_ms.push_back(frame.host_overhead_ms);
    }

    return RunAggregate {
        .frame_ms = compute_stats(frame_ms),
        .pipeline_ms = compute_stats(pipeline_ms),
        .render_ms = compute_stats(render_ms),
        .denoise_ms = compute_stats(denoise_ms),
        .download_ms = compute_stats(download_ms),
        .render_work_ms = compute_stats(render_work_ms),
        .denoise_work_ms = compute_stats(denoise_work_ms),
        .download_work_ms = compute_stats(download_work_ms),
        .image_write_ms = compute_stats(image_write_ms),
        .host_overhead_ms = compute_stats(host_overhead_ms),
    };
}

void write_csv(const RunReport& report, const std::filesystem::path& path) {
    std::ofstream out = open_output_or_throw(path, "csv");
    out << "frame_index,sample_stream,camera_count,profile,width,height,samples_per_pixel,max_"
           "bounces,denoise_enabled,"
           "frame_ms,pipeline_ms,render_ms,denoise_ms,download_ms,render_work_ms,denoise_work_ms,"
           "download_work_ms,image_write_ms,host_overhead_ms,fps\n";
    for (const FrameStageSample& frame : report.frames) {
        out << frame.frame_index << "," << frame.sample_stream << "," << frame.camera_count << ","
            << escape_csv_field(frame.profile) << "," << frame.width << "," << frame.height << ","
            << frame.samples_per_pixel << "," << frame.max_bounces << ","
            << bool_text(frame.denoise_enabled) << "," << frame.frame_ms << "," << frame.pipeline_ms
            << "," << frame.render_ms << "," << frame.denoise_ms << "," << frame.download_ms << ","
            << frame.render_work_ms << "," << frame.denoise_work_ms << "," << frame.download_work_ms
            << "," << frame.image_write_ms << "," << frame.host_overhead_ms << "," << frame.fps
            << "\n";
    }
    out.flush();
    ensure_write_ok_or_throw(out, path, "csv");
}

void write_json(const RunReport& report, const std::filesystem::path& path) {
    std::ofstream out = open_output_or_throw(path, "json");
    out << "{\n";
    out << "  \"schema_version\": " << report.schema_version << ",\n";
    out << "  \"metadata\": {\n";
    out << "    \"scene\": \"" << escape_json_string(report.scene) << "\",\n";
    out << "    \"profile\": \"" << escape_json_string(report.profile) << "\",\n";
    out << "    \"camera_count\": " << report.camera_count << ",\n";
    out << "    \"width\": " << report.width << ",\n";
    out << "    \"height\": " << report.height << ",\n";
    out << "    \"frames_requested\": " << report.frames_requested << ",\n";
    out << "    \"warmup_frames\": " << report.warmup_frames << ",\n";
    out << "    \"random_seed\": " << report.random_seed << ",\n";
    out << "    \"measurement_sample_stream_start\": "
        << static_cast<std::uint64_t>(report.random_seed)
               + static_cast<std::uint64_t>(report.warmup_frames)
        << ",\n";
    out << "    \"rng_algorithm\": \"hashed_pixel_sample_stream_v1\",\n";
    out << "    \"samples_per_pixel\": " << report.samples_per_pixel << ",\n";
    out << "    \"max_bounces\": " << report.max_bounces << ",\n";
    out << "    \"denoise_enabled\": " << bool_text(report.denoise_enabled) << ",\n";
    out << "    \"image_write_enabled\": " << bool_text(report.image_write_enabled) << "\n";
    out << "  },\n";

    out << "  \"provenance\": {\n";
    out << "    \"captured_at_utc\": \"" << escape_json_string(report.provenance.captured_at_utc)
        << "\",\n";
    out << "    \"project_version\": \"" << escape_json_string(report.provenance.project_version)
        << "\",\n";
    out << "    \"source_revision\": \"" << escape_json_string(report.provenance.source_revision)
        << "\",\n";
    out << "    \"source_dirty\": " << bool_text(report.provenance.source_dirty) << ",\n";
    out << "    \"source_scope\": \"" << escape_json_string(report.provenance.source_scope)
        << "\",\n";
    out << "    \"source_state_sha256\": \""
        << escape_json_string(report.provenance.source_state_sha256) << "\",\n";
    out << "    \"build_configuration\": \""
        << escape_json_string(report.provenance.build_configuration) << "\",\n";
    out << "    \"cxx_compiler\": \"" << escape_json_string(report.provenance.cxx_compiler)
        << "\"\n";
    out << "  },\n";

    out << "  \"environment\": {\n";
    out << "    \"operating_system\": \"" << escape_json_string(report.environment.operating_system)
        << "\",\n";
    out << "    \"architecture\": \"" << escape_json_string(report.environment.architecture)
        << "\",\n";
    out << "    \"gpu_device\": " << report.environment.gpu_device << ",\n";
    out << "    \"gpu_name\": \"" << escape_json_string(report.environment.gpu_name) << "\",\n";
    out << "    \"compute_capability\": \""
        << escape_json_string(report.environment.compute_capability) << "\",\n";
    out << "    \"nvidia_driver_version\": \""
        << escape_json_string(report.environment.nvidia_driver_version) << "\",\n";
    out << "    \"cuda_driver_api_version\": " << report.environment.cuda_driver_api_version
        << ",\n";
    out << "    \"cuda_driver_api_version_text\": \""
        << escape_json_string(report.environment.cuda_driver_api_version_text) << "\",\n";
    out << "    \"cuda_runtime_version\": " << report.environment.cuda_runtime_version << ",\n";
    out << "    \"cuda_runtime_version_text\": \""
        << escape_json_string(report.environment.cuda_runtime_version_text) << "\",\n";
    out << "    \"optix_version\": " << report.environment.optix_version << ",\n";
    out << "    \"optix_version_text\": \""
        << escape_json_string(report.environment.optix_version_text) << "\"\n";
    out << "  },\n";

    out << "  \"gpu_memory\": {\n";
    out << "    \"scope\": \"" << escape_json_string(report.gpu_memory.scope) << "\",\n";
    out << "    \"total_bytes\": " << report.gpu_memory.total_bytes << ",\n";
    out << "    \"baseline_used_bytes\": " << report.gpu_memory.baseline_used_bytes << ",\n";
    out << "    \"prepared_used_bytes\": " << report.gpu_memory.prepared_used_bytes << ",\n";
    out << "    \"peak_used_bytes\": " << report.gpu_memory.peak_used_bytes << ",\n";
    out << "    \"peak_delta_bytes\": " << report.gpu_memory.peak_delta_bytes << ",\n";
    out << "    \"final_used_bytes\": " << report.gpu_memory.final_used_bytes << "\n";
    out << "  },\n";

    out << "  \"gpu_scheduling\": {\n";
    out << "    \"persistent_worker_count\": " << report.gpu_scheduling.persistent_worker_count
        << ",\n";
    out << "    \"worker_start_count\": " << report.gpu_scheduling.worker_start_count << ",\n";
    out << "    \"task_submission_count\": " << report.gpu_scheduling.task_submission_count
        << ",\n";
    out << "    \"launch_parameter_allocation_count\": "
        << report.gpu_scheduling.launch_parameter_allocation_count << ",\n";
    out << "    \"launch_parameter_upload_count\": "
        << report.gpu_scheduling.launch_parameter_upload_count << ",\n";
    out << "    \"acceleration_update_kind\": \""
        << escape_json_string(report.gpu_scheduling.acceleration_update_kind) << "\",\n";
    out << "    \"acceleration_update_ms\": " << report.gpu_scheduling.acceleration_update_ms
        << ",\n";
    out << "    \"acceleration_node_count\": " << report.gpu_scheduling.acceleration_node_count
        << ",\n";
    out << "    \"acceleration_reference_count\": "
        << report.gpu_scheduling.acceleration_reference_count << ",\n";
    out << "    \"acceleration_prototype_count\": "
        << report.gpu_scheduling.acceleration_prototype_count << ",\n";
    out << "    \"acceleration_instance_count\": "
        << report.gpu_scheduling.acceleration_instance_count << ",\n";
    out << "    \"acceleration_instanced_primitive_count\": "
        << report.gpu_scheduling.acceleration_instanced_primitive_count << ",\n";
    out << "    \"acceleration_generation\": " << report.gpu_scheduling.acceleration_generation
        << "\n";
    out << "  },\n";

    out << "  \"aggregate\": {\n";
    write_aggregate_stats(out, "frame_ms", report.aggregate.frame_ms, true);
    write_aggregate_stats(out, "pipeline_ms", report.aggregate.pipeline_ms, true);
    write_aggregate_stats(out, "render_ms", report.aggregate.render_ms, true);
    write_aggregate_stats(out, "denoise_ms", report.aggregate.denoise_ms, true);
    write_aggregate_stats(out, "download_ms", report.aggregate.download_ms, true);
    write_aggregate_stats(out, "render_work_ms", report.aggregate.render_work_ms, true);
    write_aggregate_stats(out, "denoise_work_ms", report.aggregate.denoise_work_ms, true);
    write_aggregate_stats(out, "download_work_ms", report.aggregate.download_work_ms, true);
    write_aggregate_stats(out, "image_write_ms", report.aggregate.image_write_ms, true);
    write_aggregate_stats(out, "host_overhead_ms", report.aggregate.host_overhead_ms, false);
    out << "  },\n";

    out << "  \"frames\": [\n";
    for (std::size_t i = 0; i < report.frames.size(); ++i) {
        const FrameStageSample& frame = report.frames[i];
        out << "    {\"frame_index\": " << frame.frame_index
            << ", \"sample_stream\": " << frame.sample_stream
            << ", \"camera_count\": " << frame.camera_count << ", \"profile\": \""
            << escape_json_string(frame.profile) << "\", \"width\": " << frame.width
            << ", \"height\": " << frame.height
            << ", \"samples_per_pixel\": " << frame.samples_per_pixel
            << ", \"max_bounces\": " << frame.max_bounces
            << ", \"denoise_enabled\": " << bool_text(frame.denoise_enabled)
            << ", \"frame_ms\": " << frame.frame_ms << ", \"pipeline_ms\": " << frame.pipeline_ms
            << ", \"render_ms\": " << frame.render_ms << ", \"denoise_ms\": " << frame.denoise_ms
            << ", \"download_ms\": " << frame.download_ms
            << ", \"render_work_ms\": " << frame.render_work_ms
            << ", \"denoise_work_ms\": " << frame.denoise_work_ms
            << ", \"download_work_ms\": " << frame.download_work_ms
            << ", \"image_write_ms\": " << frame.image_write_ms
            << ", \"host_overhead_ms\": " << frame.host_overhead_ms << ", \"fps\": " << frame.fps
            << "}";
        if (i + 1U != report.frames.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"per-camera\": [\n";
    bool first_record = true;
    for (const FrameStageSample& frame : report.frames) {
        for (const CameraStageSample& camera : frame.cameras) {
            if (!first_record) {
                out << ",\n";
            }
            first_record = false;
            out << "    {\"frame_index\": " << frame.frame_index
                << ", \"camera_index\": " << camera.camera_index
                << ", \"render_ms\": " << camera.render_ms
                << ", \"denoise_ms\": " << camera.denoise_ms
                << ", \"download_ms\": " << camera.download_ms
                << ", \"average_luminance\": " << camera.average_luminance << "}";
        }
    }
    out << "\n";
    out << "  ]\n";
    out << "}\n";
    out.flush();
    ensure_write_ok_or_throw(out, path, "json");
}

void write_artifact_manifest(const RunReport& report,
    const std::vector<std::filesystem::path>& artifacts, const std::filesystem::path& path) {
    if (artifacts.empty()) {
        throw std::invalid_argument("benchmark artifact manifest requires at least one artifact");
    }

    struct ArtifactRecord {
        std::string filename;
        std::string kind;
        std::uintmax_t size_bytes = 0;
        std::string digest;
    };
    std::vector<ArtifactRecord> records;
    records.reserve(artifacts.size());
    for (const std::filesystem::path& artifact : artifacts) {
        if (!std::filesystem::is_regular_file(artifact)) {
            throw std::runtime_error(
                "benchmark artifact is not a regular file: " + artifact.string());
        }
        records.push_back(ArtifactRecord {
            .filename = artifact.filename().string(),
            .kind = artifact_kind(artifact),
            .size_bytes = std::filesystem::file_size(artifact),
            .digest = fnv1a64_file(artifact),
        });
    }

    std::ofstream out = open_output_or_throw(path, "artifact manifest");
    std::size_t per_camera_record_count = 0;
    for (const FrameStageSample& frame : report.frames) {
        per_camera_record_count += frame.cameras.size();
    }

    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"report_schema_version\": " << report.schema_version << ",\n";
    out << "  \"measured_frame_count\": " << report.frames.size() << ",\n";
    out << "  \"per_camera_record_count\": " << per_camera_record_count << ",\n";
    out << "  \"artifacts\": [\n";
    for (std::size_t i = 0; i < records.size(); ++i) {
        const ArtifactRecord& artifact = records[i];
        out << "    {\"filename\": \"" << escape_json_string(artifact.filename)
            << "\", \"kind\": \"" << escape_json_string(artifact.kind)
            << "\", \"size_bytes\": " << artifact.size_bytes
            << ", \"digest_algorithm\": \"fnv1a64\", \"digest\": \"" << artifact.digest << "\"}";
        if (i + 1U != records.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    out.flush();
    ensure_write_ok_or_throw(out, path, "artifact manifest");
}

} // namespace rt::profiling
