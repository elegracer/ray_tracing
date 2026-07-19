#include "realtime/profiling/benchmark_report.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main() {
    namespace profiling = rt::profiling;
    const std::string tricky_profile = "rt,\"x\"\\path";

    profiling::RunReport report {};
    report.provenance = profiling::RunProvenance {
        .captured_at_utc = "2026-07-18T15:00:00Z",
        .project_version = "0.0.1.0",
        .source_revision = "0123456789abcdef",
        .source_dirty = true,
        .source_scope = "CMakeLists.txt,cmake,src,utils",
        .source_state_sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        .build_configuration = "RelWithDebInfo",
        .cxx_compiler = "Clang 18.1.3",
    };
    report.environment = profiling::RunEnvironment {
        .operating_system = "Linux 6.8.0",
        .architecture = "x86_64",
        .gpu_device = 0,
        .gpu_name = "Test GPU",
        .compute_capability = "8.6",
        .nvidia_driver_version = "595.71.05",
        .cuda_driver_api_version = 13000,
        .cuda_driver_api_version_text = "13.0",
        .cuda_runtime_version = 13000,
        .cuda_runtime_version_text = "13.0",
        .optix_version = 90100,
        .optix_version_text = "9.1.0",
    };
    report.gpu_memory = profiling::GpuMemoryReport {
        .scope = "cuda_device_global",
        .total_bytes = 24ULL * 1024ULL * 1024ULL * 1024ULL,
        .baseline_used_bytes = 100,
        .prepared_used_bytes = 200,
        .peak_used_bytes = 300,
        .peak_delta_bytes = 200,
        .final_used_bytes = 250,
    };
    report.scene = "final_room";
    report.profile = tricky_profile;
    report.camera_count = 2;
    report.width = 640;
    report.height = 480;
    report.frames_requested = 2;
    report.warmup_frames = 3;
    report.random_seed = 42;
    report.samples_per_pixel = 1;
    report.max_bounces = 2;
    report.denoise_enabled = true;
    report.image_write_enabled = false;
    report.frames = {
        profiling::FrameStageSample {
            .frame_index = 0,
            .sample_stream = 45,
            .camera_count = 2,
            .profile = tricky_profile,
            .width = 640,
            .height = 480,
            .samples_per_pixel = 1,
            .max_bounces = 2,
            .denoise_enabled = true,
            .frame_ms = 10.0,
            .pipeline_ms = 9.0,
            .render_ms = 3.0,
            .denoise_ms = 1.0,
            .download_ms = 0.5,
            .render_work_ms = 6.0,
            .denoise_work_ms = 2.0,
            .download_work_ms = 1.0,
            .image_write_ms = 0.5,
            .host_overhead_ms = 0.5,
            .fps = 100.0,
            .cameras =
                {
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
            .sample_stream = 46,
            .camera_count = 2,
            .profile = tricky_profile,
            .width = 640,
            .height = 480,
            .samples_per_pixel = 1,
            .max_bounces = 2,
            .denoise_enabled = true,
            .frame_ms = 14.0,
            .pipeline_ms = 13.0,
            .render_ms = 4.0,
            .denoise_ms = 1.5,
            .download_ms = 0.75,
            .render_work_ms = 8.0,
            .denoise_work_ms = 3.0,
            .download_work_ms = 1.5,
            .image_write_ms = 1.0,
            .host_overhead_ms = 0.0,
            .fps = 71.428571,
            .cameras =
                {
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
    expect_near(report.aggregate.frame_ms.p99, 14.0, 1e-12, "frame p99");
    expect_near(report.aggregate.pipeline_ms.avg, 11.0, 1e-12, "pipeline avg");
    expect_near(report.aggregate.denoise_ms.max, 1.5, 1e-12, "denoise critical path max");
    expect_near(report.aggregate.denoise_work_ms.max, 3.0, 1e-12, "denoise work max");
    expect_near(report.aggregate.host_overhead_ms.avg, 0.25, 1e-12, "host residual avg");

    const std::filesystem::path out_dir =
        std::filesystem::temp_directory_path() / "rt-benchmark-report-test";
    std::filesystem::remove_all(out_dir);
    std::filesystem::create_directories(out_dir);
    const std::filesystem::path csv_path = out_dir / "benchmark_frames.csv";
    const std::filesystem::path json_path = out_dir / "benchmark_summary.json";
    const std::filesystem::path manifest_path = out_dir / "benchmark_manifest.json";

    profiling::write_csv(report, csv_path);
    profiling::write_json(report, json_path);
    profiling::write_artifact_manifest(report, {csv_path, json_path}, manifest_path);

    std::ifstream csv(csv_path);
    expect_true(csv.is_open(), "csv file is readable");
    const std::string csv_text((std::istreambuf_iterator<char>(csv)),
        std::istreambuf_iterator<char>());
    expect_true(!csv_text.empty(), "csv is non-empty");
    expect_true(csv_text.find("frame_index,sample_stream,camera_count,profile,width,height")
                    != std::string::npos,
        "csv header");
    expect_true(csv_text.find("\"rt,\"\"x\"\"\\path\"") != std::string::npos, "csv quoted profile");
    expect_true(csv_text.find("0,45,2,\"rt,\"\"x\"\"\\path\",640,480") != std::string::npos,
        "csv first row");

    std::ifstream json(json_path);
    expect_true(json.is_open(), "json file is readable");
    const std::string json_text((std::istreambuf_iterator<char>(json)),
        std::istreambuf_iterator<char>());
    expect_true(!json_text.empty(), "json is non-empty");
    expect_true(json_text.find("\"metadata\"") != std::string::npos, "json metadata field");
    expect_true(json_text.find("\"schema_version\": 3") != std::string::npos,
        "json schema version");
    expect_true(json_text.find("\"scene\": \"final_room\"") != std::string::npos,
        "json scene field");
    expect_true(json_text.find("\"aggregate\"") != std::string::npos, "json aggregate field");
    expect_true(json_text.find("\"frames\"") != std::string::npos, "json frames field");
    expect_true(json_text.find("\"per-camera\"") != std::string::npos, "json per-camera field");
    expect_true(json_text.find("\"profile\": \"rt,\\\"x\\\"\\\\path\"") != std::string::npos,
        "json escaped profile");
    expect_true(json_text.find("\"frame_ms\"") != std::string::npos, "json aggregate");
    expect_true(json_text.find("\"p99\": 14") != std::string::npos, "json p99 aggregate");
    expect_true(json_text.find("\"warmup_frames\": 3") != std::string::npos,
        "json warmup metadata");
    expect_true(json_text.find("\"random_seed\": 42") != std::string::npos, "json random seed");
    expect_true(json_text.find("\"sample_stream\": 45") != std::string::npos, "json sample stream");
    expect_true(json_text.find("\"provenance\"") != std::string::npos, "json provenance");
    expect_true(json_text.find("\"source_revision\": \"0123456789abcdef\"") != std::string::npos,
        "json source revision");
    expect_true(json_text.find("\"source_scope\": \"CMakeLists.txt,cmake,src,utils\"")
                    != std::string::npos,
        "json source scope");
    expect_true(json_text.find(
                    "\"source_state_sha256\": "
                    "\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"")
                    != std::string::npos,
        "json source state fingerprint");
    expect_true(json_text.find("\"gpu_name\": \"Test GPU\"") != std::string::npos,
        "json GPU environment");
    expect_true(json_text.find("\"peak_used_bytes\": 300") != std::string::npos, "json GPU memory");
    expect_true(json_text.find("\"peak_delta_bytes\": 200") != std::string::npos,
        "json GPU memory delta");
    expect_true(json_text.find("\"nvidia_driver_version\": \"595.71.05\"") != std::string::npos,
        "json NVIDIA driver version");
    expect_true(json_text.find("\"pipeline_ms\"") != std::string::npos, "json pipeline timing");
    expect_true(json_text.find("\"render_work_ms\"") != std::string::npos,
        "json render work timing");
    expect_true(json_text.find("\"host_overhead_ms\": 0.5") != std::string::npos,
        "json frame host overhead");
    expect_true(json_text.find("\"host_overhead_ms\": 0") != std::string::npos,
        "json non-negative host residual");
    expect_true(json_text.find("\"fps\": 100") != std::string::npos, "json frame fps");
    expect_true(json_text.find("\"camera_index\": 1") != std::string::npos, "json per camera");

    std::ifstream manifest(manifest_path);
    expect_true(manifest.is_open(), "manifest file is readable");
    const std::string manifest_text((std::istreambuf_iterator<char>(manifest)),
        std::istreambuf_iterator<char>());
    expect_true(manifest_text.find("\"report_schema_version\": 3") != std::string::npos,
        "manifest report schema");
    expect_true(manifest_text.find("\"measured_frame_count\": 2") != std::string::npos,
        "manifest frame count");
    expect_true(manifest_text.find("\"per_camera_record_count\": 4") != std::string::npos,
        "manifest per-camera count");
    expect_true(manifest_text.find("\"filename\": \"benchmark_frames.csv\"") != std::string::npos,
        "manifest CSV artifact");
    expect_true(manifest_text.find("\"filename\": \"benchmark_summary.json\"") != std::string::npos,
        "manifest JSON artifact");
    expect_true(manifest_text.find("\"digest_algorithm\": \"fnv1a64\"") != std::string::npos,
        "manifest digest algorithm");

    const std::filesystem::path known_artifact_path = out_dir / "known.txt";
    {
        std::ofstream known_artifact(known_artifact_path, std::ios::binary);
        known_artifact << "hello";
    }
    const std::filesystem::path known_manifest_path = out_dir / "known-manifest.json";
    profiling::write_artifact_manifest(report, {known_artifact_path}, known_manifest_path);
    std::ifstream known_manifest(known_manifest_path);
    const std::string known_manifest_text((std::istreambuf_iterator<char>(known_manifest)),
        std::istreambuf_iterator<char>());
    expect_true(known_manifest_text.find("\"digest\": \"a430d84680aabd0b\"") != std::string::npos,
        "manifest FNV-1a digest matches the independent hello test vector");

    const std::filesystem::path bad_csv_path = out_dir / "missing-parent" / "benchmark_frames.csv";
    const std::filesystem::path bad_json_path =
        out_dir / "missing-parent" / "benchmark_summary.json";
    bool csv_failed = false;
    try {
        profiling::write_csv(report, bad_csv_path);
    } catch (...) { csv_failed = true; }
    expect_true(csv_failed, "csv write failure throws");

    bool json_failed = false;
    try {
        profiling::write_json(report, bad_json_path);
    } catch (...) { json_failed = true; }
    expect_true(json_failed, "json write failure throws");

    bool manifest_output_failed = false;
    try {
        profiling::write_artifact_manifest(report, {csv_path, json_path},
            out_dir / "missing-parent" / "manifest.json");
    } catch (...) { manifest_output_failed = true; }
    expect_true(manifest_output_failed, "manifest write failure throws");

    bool manifest_input_failed = false;
    try {
        profiling::write_artifact_manifest(report, {csv_path, out_dir / "missing.json"},
            out_dir / "invalid-manifest.json");
    } catch (...) { manifest_input_failed = true; }
    expect_true(manifest_input_failed, "manifest rejects missing artifacts");
    expect_true(!std::filesystem::exists(out_dir / "invalid-manifest.json"),
        "manifest validates every input before creating output");
    return 0;
}
