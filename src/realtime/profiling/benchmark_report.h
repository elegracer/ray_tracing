#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rt::profiling {

struct CameraStageSample {
    int camera_index = 0;
    double render_ms = 0.0;
    double denoise_ms = 0.0;
    double download_ms = 0.0;
    double average_luminance = 0.0;
};

struct FrameStageSample {
    int frame_index = 0;
    std::uint32_t sample_stream = 0;
    int camera_count = 0;
    std::string profile;
    int width = 0;
    int height = 0;
    int samples_per_pixel = 0;
    int max_bounces = 0;
    bool denoise_enabled = false;
    double frame_ms = 0.0;
    // Wall-clock critical path for parallel render, denoise, and download work.
    double pipeline_ms = 0.0;
    // Longest per-camera stage duration; cameras execute concurrently.
    double render_ms = 0.0;
    double denoise_ms = 0.0;
    double download_ms = 0.0;
    // Sum of per-camera GPU work, useful for utilization rather than latency.
    double render_work_ms = 0.0;
    double denoise_work_ms = 0.0;
    double download_work_ms = 0.0;
    double image_write_ms = 0.0;
    // Non-negative wall residual outside pipeline execution and image writes.
    double host_overhead_ms = 0.0;
    double fps = 0.0;
    std::vector<CameraStageSample> cameras;
};

struct AggregateStats {
    double avg = 0.0;
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double max = 0.0;
};

struct RunAggregate {
    AggregateStats frame_ms;
    AggregateStats pipeline_ms;
    AggregateStats render_ms;
    AggregateStats denoise_ms;
    AggregateStats download_ms;
    AggregateStats render_work_ms;
    AggregateStats denoise_work_ms;
    AggregateStats download_work_ms;
    AggregateStats image_write_ms;
    AggregateStats host_overhead_ms;
};

struct RunProvenance {
    std::string captured_at_utc;
    std::string project_version;
    std::string source_revision;
    bool source_dirty = false;
    std::string source_scope;
    std::string source_state_sha256;
    std::string build_configuration;
    std::string cxx_compiler;
};

struct RunEnvironment {
    std::string operating_system;
    std::string architecture;
    int gpu_device = 0;
    std::string gpu_name;
    std::string compute_capability;
    std::string nvidia_driver_version;
    int cuda_driver_api_version = 0;
    std::string cuda_driver_api_version_text;
    int cuda_runtime_version = 0;
    std::string cuda_runtime_version_text;
    int optix_version = 0;
    std::string optix_version_text;
};

struct GpuMemoryReport {
    std::string scope = "cuda_device_global";
    std::uint64_t total_bytes = 0;
    std::uint64_t baseline_used_bytes = 0;
    std::uint64_t prepared_used_bytes = 0;
    std::uint64_t peak_used_bytes = 0;
    std::uint64_t peak_delta_bytes = 0;
    std::uint64_t final_used_bytes = 0;
};

struct GpuSchedulingReport {
    int persistent_worker_count = 0;
    std::uint64_t worker_start_count = 0;
    std::uint64_t task_submission_count = 0;
    std::uint64_t launch_parameter_allocation_count = 0;
    std::uint64_t launch_parameter_upload_count = 0;
    std::string acceleration_update_kind;
    double acceleration_update_ms = 0.0;
    int acceleration_node_count = 0;
    int acceleration_reference_count = 0;
    int acceleration_prototype_count = 0;
    int acceleration_instance_count = 0;
    int acceleration_instanced_primitive_count = 0;
    std::uint64_t acceleration_generation = 0;
};

struct RunReport {
    int schema_version = 3;
    RunProvenance provenance;
    RunEnvironment environment;
    GpuMemoryReport gpu_memory;
    GpuSchedulingReport gpu_scheduling;
    std::string scene;
    std::string profile;
    int camera_count = 0;
    int width = 0;
    int height = 0;
    int frames_requested = 0;
    int warmup_frames = 0;
    std::uint32_t random_seed = 0;
    int samples_per_pixel = 0;
    int max_bounces = 0;
    bool denoise_enabled = false;
    bool image_write_enabled = true;
    std::vector<FrameStageSample> frames;
    RunAggregate aggregate;
};

RunAggregate compute_aggregate(const std::vector<FrameStageSample>& frames);
void write_csv(const RunReport& report, const std::filesystem::path& path);
void write_json(const RunReport& report, const std::filesystem::path& path);
void write_artifact_manifest(const RunReport& report,
    const std::vector<std::filesystem::path>& artifacts, const std::filesystem::path& path);

} // namespace rt::profiling
