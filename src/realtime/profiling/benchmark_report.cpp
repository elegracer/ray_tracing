#include "realtime/profiling/benchmark_report.h"

#include <algorithm>
#include <cmath>
#include <fstream>
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
    stats.max = values.back();
    return stats;
}

const char* bool_text(bool value) {
    return value ? "true" : "false";
}

void write_aggregate_stats(std::ofstream& out, const char* name, const AggregateStats& stats, bool trailing_comma) {
    out << "    \"" << name << "\": {\"avg\": " << stats.avg << ", \"p50\": " << stats.p50 << ", \"p95\": " << stats.p95
        << ", \"max\": " << stats.max << "}" << (trailing_comma ? "," : "") << "\n";
}

}  // namespace

RunAggregate compute_aggregate(const std::vector<FrameStageSample>& frames) {
    std::vector<double> frame_ms;
    std::vector<double> render_ms;
    std::vector<double> denoise_ms;
    std::vector<double> download_ms;
    std::vector<double> image_write_ms;
    std::vector<double> host_overhead_ms;

    frame_ms.reserve(frames.size());
    render_ms.reserve(frames.size());
    denoise_ms.reserve(frames.size());
    download_ms.reserve(frames.size());
    image_write_ms.reserve(frames.size());
    host_overhead_ms.reserve(frames.size());

    for (const FrameStageSample& frame : frames) {
        frame_ms.push_back(frame.frame_ms);
        render_ms.push_back(frame.render_ms);
        denoise_ms.push_back(frame.denoise_ms);
        download_ms.push_back(frame.download_ms);
        image_write_ms.push_back(frame.image_write_ms);
        host_overhead_ms.push_back(frame.host_overhead_ms);
    }

    return RunAggregate {
        .frame_ms = compute_stats(frame_ms),
        .render_ms = compute_stats(render_ms),
        .denoise_ms = compute_stats(denoise_ms),
        .download_ms = compute_stats(download_ms),
        .image_write_ms = compute_stats(image_write_ms),
        .host_overhead_ms = compute_stats(host_overhead_ms),
    };
}

void write_csv(const RunReport& report, const std::filesystem::path& path) {
    std::ofstream out(path);
    out << "frame_index,camera_count,profile,width,height,samples_per_pixel,max_bounces,denoise_enabled,"
           "frame_ms,render_ms,denoise_ms,download_ms,image_write_ms,host_overhead_ms,fps\n";
    for (const FrameStageSample& frame : report.frames) {
        out << frame.frame_index << "," << frame.camera_count << "," << frame.profile << "," << frame.width << ","
            << frame.height << "," << frame.samples_per_pixel << "," << frame.max_bounces << ","
            << bool_text(frame.denoise_enabled) << "," << frame.frame_ms << "," << frame.render_ms << ","
            << frame.denoise_ms << "," << frame.download_ms << "," << frame.image_write_ms << ","
            << frame.host_overhead_ms << "," << frame.fps << "\n";
    }
}

void write_json(const RunReport& report, const std::filesystem::path& path) {
    std::ofstream out(path);
    out << "{\n";
    out << "  \"metadata\": {\n";
    out << "    \"profile\": \"" << report.profile << "\",\n";
    out << "    \"camera_count\": " << report.camera_count << ",\n";
    out << "    \"width\": " << report.width << ",\n";
    out << "    \"height\": " << report.height << ",\n";
    out << "    \"frames_requested\": " << report.frames_requested << ",\n";
    out << "    \"samples_per_pixel\": " << report.samples_per_pixel << ",\n";
    out << "    \"max_bounces\": " << report.max_bounces << ",\n";
    out << "    \"denoise_enabled\": " << bool_text(report.denoise_enabled) << "\n";
    out << "  },\n";

    out << "  \"aggregate\": {\n";
    write_aggregate_stats(out, "frame_ms", report.aggregate.frame_ms, true);
    write_aggregate_stats(out, "render_ms", report.aggregate.render_ms, true);
    write_aggregate_stats(out, "denoise_ms", report.aggregate.denoise_ms, true);
    write_aggregate_stats(out, "download_ms", report.aggregate.download_ms, true);
    write_aggregate_stats(out, "image_write_ms", report.aggregate.image_write_ms, true);
    write_aggregate_stats(out, "host_overhead_ms", report.aggregate.host_overhead_ms, false);
    out << "  },\n";

    out << "  \"per-camera\": [\n";
    bool first_record = true;
    for (const FrameStageSample& frame : report.frames) {
        for (const CameraStageSample& camera : frame.cameras) {
            if (!first_record) {
                out << ",\n";
            }
            first_record = false;
            out << "    {\"frame_index\": " << frame.frame_index << ", \"camera_index\": " << camera.camera_index
                << ", \"render_ms\": " << camera.render_ms << ", \"denoise_ms\": " << camera.denoise_ms
                << ", \"download_ms\": " << camera.download_ms << ", \"average_luminance\": " << camera.average_luminance
                << "}";
        }
    }
    out << "\n";
    out << "  ]\n";
    out << "}\n";
}

}  // namespace rt::profiling
