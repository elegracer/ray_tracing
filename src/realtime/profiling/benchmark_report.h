#pragma once

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
    int camera_count = 0;
    std::string profile;
    int width = 0;
    int height = 0;
    int samples_per_pixel = 0;
    int max_bounces = 0;
    bool denoise_enabled = false;
    double frame_ms = 0.0;
    double render_ms = 0.0;
    double denoise_ms = 0.0;
    double download_ms = 0.0;
    double image_write_ms = 0.0;
    double host_overhead_ms = 0.0;
    double fps = 0.0;
    std::vector<CameraStageSample> cameras;
};

struct AggregateStats {
    double avg = 0.0;
    double p50 = 0.0;
    double p95 = 0.0;
    double max = 0.0;
};

struct RunAggregate {
    AggregateStats frame_ms;
    AggregateStats render_ms;
    AggregateStats denoise_ms;
    AggregateStats download_ms;
    AggregateStats image_write_ms;
    AggregateStats host_overhead_ms;
};

struct RunReport {
    std::string profile;
    int camera_count = 0;
    int width = 0;
    int height = 0;
    int frames_requested = 0;
    int samples_per_pixel = 0;
    int max_bounces = 0;
    bool denoise_enabled = false;
    std::vector<FrameStageSample> frames;
    RunAggregate aggregate;
};

RunAggregate compute_aggregate(const std::vector<FrameStageSample>& frames);
void write_csv(const RunReport& report, const std::filesystem::path& path);
void write_json(const RunReport& report, const std::filesystem::path& path);

}  // namespace rt::profiling
