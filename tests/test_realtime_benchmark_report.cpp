#include "realtime/profiling/benchmark_report.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main() {
    namespace profiling = rt::profiling;

    profiling::RunReport report {};
    report.profile = "realtime";
    report.camera_count = 2;
    report.width = 640;
    report.height = 480;
    report.frames_requested = 2;
    report.samples_per_pixel = 1;
    report.max_bounces = 2;
    report.denoise_enabled = true;
    report.frames = {
        profiling::FrameStageSample {
            .frame_index = 0,
            .camera_count = 2,
            .profile = "realtime",
            .width = 640,
            .height = 480,
            .samples_per_pixel = 1,
            .max_bounces = 2,
            .denoise_enabled = true,
            .frame_ms = 10.0,
            .render_ms = 6.0,
            .denoise_ms = 2.0,
            .download_ms = 1.0,
            .image_write_ms = 0.5,
            .host_overhead_ms = 0.5,
            .fps = 100.0,
            .cameras = {
                profiling::CameraStageSample {
                    .camera_index = 0,
                    .render_ms = 3.0,
                    .denoise_ms = 1.0,
                    .download_ms = 0.5,
                    .average_luminance = 0.12,
                },
                profiling::CameraStageSample {
                    .camera_index = 1,
                    .render_ms = 3.0,
                    .denoise_ms = 1.0,
                    .download_ms = 0.5,
                    .average_luminance = 0.13,
                },
            },
        },
        profiling::FrameStageSample {
            .frame_index = 1,
            .camera_count = 2,
            .profile = "realtime",
            .width = 640,
            .height = 480,
            .samples_per_pixel = 1,
            .max_bounces = 2,
            .denoise_enabled = true,
            .frame_ms = 14.0,
            .render_ms = 8.0,
            .denoise_ms = 3.0,
            .download_ms = 1.5,
            .image_write_ms = 1.0,
            .host_overhead_ms = 0.5,
            .fps = 71.428571,
            .cameras = {
                profiling::CameraStageSample {
                    .camera_index = 0,
                    .render_ms = 4.0,
                    .denoise_ms = 1.5,
                    .download_ms = 0.75,
                    .average_luminance = 0.14,
                },
                profiling::CameraStageSample {
                    .camera_index = 1,
                    .render_ms = 4.0,
                    .denoise_ms = 1.5,
                    .download_ms = 0.75,
                    .average_luminance = 0.15,
                },
            },
        },
    };

    report.aggregate = profiling::compute_aggregate(report.frames);
    expect_near(report.aggregate.frame_ms.avg, 12.0, 1e-12, "frame avg");
    expect_near(report.aggregate.frame_ms.p50, 10.0, 1e-12, "frame p50");
    expect_near(report.aggregate.frame_ms.p95, 14.0, 1e-12, "frame p95");
    expect_near(report.aggregate.denoise_ms.max, 3.0, 1e-12, "denoise max");

    const std::filesystem::path out_dir = std::filesystem::temp_directory_path() / "rt-benchmark-report-test";
    std::filesystem::create_directories(out_dir);
    const std::filesystem::path csv_path = out_dir / "benchmark_frames.csv";
    const std::filesystem::path json_path = out_dir / "benchmark_summary.json";

    profiling::write_csv(report, csv_path);
    profiling::write_json(report, json_path);

    std::ifstream csv(csv_path);
    const std::string csv_text((std::istreambuf_iterator<char>(csv)), std::istreambuf_iterator<char>());
    expect_true(csv_text.find("frame_index,camera_count,profile,width,height") != std::string::npos, "csv header");
    expect_true(csv_text.find("0,2,realtime,640,480") != std::string::npos, "csv first row");

    std::ifstream json(json_path);
    const std::string json_text((std::istreambuf_iterator<char>(json)), std::istreambuf_iterator<char>());
    expect_true(json_text.find("\"metadata\"") != std::string::npos, "json metadata field");
    expect_true(json_text.find("\"aggregate\"") != std::string::npos, "json aggregate field");
    expect_true(json_text.find("\"per-camera\"") != std::string::npos, "json per-camera field");
    expect_true(json_text.find("\"profile\": \"realtime\"") != std::string::npos, "json metadata");
    expect_true(json_text.find("\"frame_ms\"") != std::string::npos, "json aggregate");
    expect_true(json_text.find("\"camera_index\": 1") != std::string::npos, "json per camera");
    return 0;
}
