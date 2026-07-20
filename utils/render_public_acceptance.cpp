#include "realtime/camera_models.h"
#include "realtime/camera_rig.h"
#include "realtime/display_transfer.h"
#include "realtime/gpu/renderer_pool.h"
#include "realtime/render_profile.h"
#include "scene/materialx_openpbr_loader.h"
#include "scene/openpbr_core_adapter.h"
#include "scene/openusd_stage_importer.h"
#include "scene/realtime_scene_adapter.h"

#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <openssl/evp.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>

#include <Eigen/Geometry>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef RT_ACCEPTANCE_USD_REVISION
#define RT_ACCEPTANCE_USD_REVISION "unknown"
#endif
#ifndef RT_ACCEPTANCE_OPENPBR_REVISION
#define RT_ACCEPTANCE_OPENPBR_REVISION "unknown"
#endif
#ifndef RT_ACCEPTANCE_USD_ENTRYPOINT
#define RT_ACCEPTANCE_USD_ENTRYPOINT "unknown"
#endif

namespace {

using json = nlohmann::json;

constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr std::uint32_t kSampleSeed = 20260721U;
constexpr double kLinearRmseThreshold = 0.005;
constexpr double kLinearMaxAbsThreshold = 0.05;
constexpr double kDisplayMeanAbsThreshold = 1.0;
constexpr double kDisplayMaxAbsThreshold = 8.0;

struct Bounds {
    Eigen::Vector3d min = Eigen::Vector3d::Constant(std::numeric_limits<double>::infinity());
    Eigen::Vector3d max = Eigen::Vector3d::Constant(-std::numeric_limits<double>::infinity());

    void include(const Eigen::Vector3d& point) {
        min = min.cwiseMin(point);
        max = max.cwiseMax(point);
    }
};

struct ViewSpec {
    std::string id;
    std::string pose;
    rt::CameraModelType model = rt::CameraModelType::pinhole32;
    double azimuth_degrees = 0.0;
    double elevation_ratio = 0.0;
    int camera_index = 0;
    bool simultaneous = false;
};

struct ArtifactPair {
    std::filesystem::path exr;
    std::filesystem::path png;
    json output;
    json reference;
};

std::string model_name(rt::CameraModelType model) {
    switch (model) {
        case rt::CameraModelType::pinhole32: return "pinhole32";
        case rt::CameraModelType::equi62_lut1d: return "equi62_lut1d";
    }
    throw std::invalid_argument("unknown camera model");
}

double radians(double degrees) {
    return degrees * std::numbers::pi / 180.0;
}

Bounds scene_bounds(const rt::PackedScene& scene) {
    Bounds bounds;
    for (const rt::SpherePrimitive& sphere : scene.spheres) {
        bounds.include(sphere.center - Eigen::Vector3d::Constant(sphere.radius));
        bounds.include(sphere.center + Eigen::Vector3d::Constant(sphere.radius));
    }
    for (const rt::QuadPrimitive& quad : scene.quads) {
        bounds.include(quad.origin);
        bounds.include(quad.origin + quad.edge_u);
        bounds.include(quad.origin + quad.edge_v);
        bounds.include(quad.origin + quad.edge_u + quad.edge_v);
    }
    for (const rt::TrianglePrimitive& triangle : scene.triangles) {
        bounds.include(triangle.p0);
        bounds.include(triangle.p1);
        bounds.include(triangle.p2);
    }
    if (!bounds.min.allFinite() || !bounds.max.allFinite()
        || (bounds.max - bounds.min).maxCoeff() <= 1e-9) {
        throw std::runtime_error("realtime USD scene has no finite, non-degenerate render bounds");
    }
    return bounds;
}

Eigen::Matrix3d look_at_rotation(const Eigen::Vector3d& position, const Eigen::Vector3d& target,
    const Eigen::Vector3d& up) {
    const Eigen::Vector3d forward = (target - position).normalized();
    const Eigen::Vector3d right = forward.cross(up).normalized();
    const Eigen::Vector3d down = forward.cross(right).normalized();
    Eigen::Matrix3d rotation;
    rotation.col(0) = right;
    rotation.col(1) = down;
    rotation.col(2) = forward;
    if (rotation.determinant() < 0.999999) {
        throw std::runtime_error("acceptance camera look-at frame is not a proper rotation");
    }
    return rotation;
}

rt::PackedCamera make_camera(const ViewSpec& view, const Bounds& bounds,
    rt::scene::SceneUpAxis up_axis) {
    const Eigen::Vector3d center = 0.5 * (bounds.min + bounds.max);
    const double radius = std::max(0.5 * (bounds.max - bounds.min).norm(), 1e-3);
    const Eigen::Vector3d up =
        up_axis == rt::scene::SceneUpAxis::z ? Eigen::Vector3d::UnitZ() : Eigen::Vector3d::UnitY();
    const Eigen::Vector3d axis_a = Eigen::Vector3d::UnitX();
    const Eigen::Vector3d axis_b =
        up_axis == rt::scene::SceneUpAxis::z ? Eigen::Vector3d::UnitY() : Eigen::Vector3d::UnitZ();
    const double azimuth = radians(view.azimuth_degrees);
    const Eigen::Vector3d offset_direction =
        (std::cos(azimuth) * axis_a + std::sin(azimuth) * axis_b + view.elevation_ratio * up)
            .normalized();
    const Eigen::Vector3d position = center + 3.6 * radius * offset_direction;

    rt::PackedCamera camera;
    camera.enabled = 1;
    camera.width = kWidth;
    camera.height = kHeight;
    camera.model = view.model;
    camera.T_rc = Sophus::SE3d(look_at_rotation(position, center, up), position);
    if (view.model == rt::CameraModelType::pinhole32) {
        const rt::DefaultCameraIntrinsics intrinsics =
            rt::derive_default_camera_intrinsics(view.model, kWidth, kHeight, 52.0);
        camera.pinhole = rt::Pinhole32Params {intrinsics.fx, intrinsics.fy, intrinsics.cx,
            intrinsics.cy, 0.0, 0.0, 0.0, 0.0, 0.0};
    } else {
        const rt::DefaultCameraIntrinsics intrinsics =
            rt::derive_default_camera_intrinsics(view.model, kWidth, kHeight, 80.0);
        camera.equi = rt::make_equi62_lut1d_params(kWidth, kHeight, intrinsics.fx, intrinsics.fy,
            intrinsics.cx, intrinsics.cy, std::array<double, 6> {}, Eigen::Vector2d::Zero());
    }
    return camera;
}

json camera_json(const ViewSpec& view, const rt::PackedCamera& camera) {
    const Eigen::Matrix3d rotation = camera.T_rc.rotationMatrix();
    const Eigen::Vector3d translation = camera.T_rc.translation();
    json transform = json::array();
    for (int row = 0; row < 4; ++row) {
        json values = json::array();
        for (int col = 0; col < 4; ++col) {
            const double value = row < 3 && col < 3     ? rotation(row, col)
                                 : row < 3 && col == 3  ? translation(row)
                                 : row == 3 && col == 3 ? 1.0
                                                        : 0.0;
            values.push_back(value);
        }
        transform.push_back(std::move(values));
    }
    json intrinsics;
    if (camera.model == rt::CameraModelType::pinhole32) {
        intrinsics = {{"fx", camera.pinhole.fx}, {"fy", camera.pinhole.fy},
            {"cx", camera.pinhole.cx}, {"cy", camera.pinhole.cy},
            {"radial", {camera.pinhole.k1, camera.pinhole.k2, camera.pinhole.k3}},
            {"tangential", {camera.pinhole.p1, camera.pinhole.p2}}};
    } else {
        intrinsics = {{"fx", camera.equi.fx}, {"fy", camera.equi.fy}, {"cx", camera.equi.cx},
            {"cy", camera.equi.cy}, {"radial", camera.equi.radial},
            {"tangential", {camera.equi.tangential.x(), camera.equi.tangential.y()}},
            {"lut_step", camera.equi.lut_step}, {"lut_entries", camera.equi.lut.size()}};
    }
    return {{"view_id", view.id}, {"pose", view.pose}, {"model", model_name(view.model)},
        {"width", camera.width}, {"height", camera.height},
        {"camera_to_world", std::move(transform)}, {"intrinsics", std::move(intrinsics)},
        {"simultaneous", view.simultaneous}, {"camera_index", view.camera_index}};
}

cv::Mat linear_bgr(const rt::RadianceFrame& frame) {
    cv::Mat image(frame.height, frame.width, CV_32FC3);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::size_t rgba = static_cast<std::size_t>(y * frame.width + x) * 4U;
            image.at<cv::Vec3f>(y, x) = {frame.beauty_rgba[rgba + 2], frame.beauty_rgba[rgba + 1],
                frame.beauty_rgba[rgba]};
        }
    }
    return image;
}

cv::Mat display_bgr(const rt::RadianceFrame& frame) {
    cv::Mat image(frame.height, frame.width, CV_8UC3);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::size_t rgba = static_cast<std::size_t>(y * frame.width + x) * 4U;
            image.at<cv::Vec3b>(y, x) = {rt::linear_to_display_u8(frame.beauty_rgba[rgba + 2]),
                rt::linear_to_display_u8(frame.beauty_rgba[rgba + 1]),
                rt::linear_to_display_u8(frame.beauty_rgba[rgba])};
        }
    }
    return image;
}

void write_linear_exr(const std::filesystem::path& path, const cv::Mat& image) {
    if (image.type() != CV_32FC3 || !image.isContinuous()) {
        throw std::invalid_argument("linear EXR source must be contiguous CV_32FC3");
    }
    namespace exr = OPENEXR_IMF_NAMESPACE;
    exr::Header header(image.cols, image.rows);
    header.channels().insert("R", exr::Channel(exr::FLOAT));
    header.channels().insert("G", exr::Channel(exr::FLOAT));
    header.channels().insert("B", exr::Channel(exr::FLOAT));
    exr::FrameBuffer frame_buffer;
    char* base = reinterpret_cast<char*>(image.data);
    const std::size_t pixel_stride = sizeof(cv::Vec3f);
    const std::size_t row_stride = image.step[0];
    frame_buffer.insert("B", exr::Slice(exr::FLOAT, base, pixel_stride, row_stride));
    frame_buffer.insert("G",
        exr::Slice(exr::FLOAT, base + sizeof(float), pixel_stride, row_stride));
    frame_buffer.insert("R",
        exr::Slice(exr::FLOAT, base + 2 * sizeof(float), pixel_stride, row_stride));
    exr::OutputFile output(path.c_str(), header);
    output.setFrameBuffer(frame_buffer);
    output.writePixels(image.rows);
}

cv::Mat read_linear_exr(const std::filesystem::path& path) {
    namespace exr = OPENEXR_IMF_NAMESPACE;
    exr::InputFile input(path.c_str());
    const IMATH_NAMESPACE::Box2i window = input.header().dataWindow();
    if (window.min.x != 0 || window.min.y != 0) {
        throw std::runtime_error("acceptance EXR references require a zero-origin data window");
    }
    const int width = window.max.x + 1;
    const int height = window.max.y + 1;
    cv::Mat image(height, width, CV_32FC3);
    exr::FrameBuffer frame_buffer;
    char* base = reinterpret_cast<char*>(image.data);
    const std::size_t pixel_stride = sizeof(cv::Vec3f);
    const std::size_t row_stride = image.step[0];
    frame_buffer.insert("B", exr::Slice(exr::FLOAT, base, pixel_stride, row_stride));
    frame_buffer.insert("G",
        exr::Slice(exr::FLOAT, base + sizeof(float), pixel_stride, row_stride));
    frame_buffer.insert("R",
        exr::Slice(exr::FLOAT, base + 2 * sizeof(float), pixel_stride, row_stride));
    input.setFrameBuffer(frame_buffer);
    input.readPixels(window.min.y, window.max.y);
    return image;
}

std::string sha256(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to hash artifact: " + path.string());
    }
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(),
        EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("failed to initialize SHA-256");
    }
    std::array<char, 64 * 1024> bytes {};
    while (input) {
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (input.gcount() > 0
            && EVP_DigestUpdate(context.get(), bytes.data(),
                   static_cast<std::size_t>(input.gcount()))
                   != 1) {
            throw std::runtime_error("failed to update SHA-256");
        }
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest {};
    unsigned int size = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &size) != 1) {
        throw std::runtime_error("failed to finalize SHA-256");
    }
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < size; ++i) {
        result << std::setw(2) << static_cast<unsigned int>(digest[i]);
    }
    return result.str();
}

json compare_linear(const cv::Mat& actual, const cv::Mat& reference) {
    if (actual.size() != reference.size() || actual.type() != reference.type()
        || actual.type() != CV_32FC3) {
        throw std::runtime_error("linear EXR reference shape/type mismatch");
    }
    cv::Mat difference;
    cv::absdiff(actual, reference, difference);
    double max_abs = 0.0;
    cv::minMaxLoc(difference.reshape(1), nullptr, &max_abs);
    cv::Mat squared;
    cv::multiply(difference, difference, squared);
    const cv::Scalar channel_mean = cv::mean(squared);
    const double rmse = std::sqrt((channel_mean[0] + channel_mean[1] + channel_mean[2]) / 3.0);
    const bool passed = rmse <= kLinearRmseThreshold && max_abs <= kLinearMaxAbsThreshold;
    return {{"rmse", rmse}, {"max_abs", max_abs}, {"rmse_threshold", kLinearRmseThreshold},
        {"max_abs_threshold", kLinearMaxAbsThreshold}, {"passed", passed}};
}

json compare_display(const cv::Mat& actual, const cv::Mat& reference) {
    if (actual.size() != reference.size() || actual.type() != reference.type()
        || actual.type() != CV_8UC3) {
        throw std::runtime_error("display PNG reference shape/type mismatch");
    }
    cv::Mat difference;
    cv::absdiff(actual, reference, difference);
    double max_abs = 0.0;
    cv::minMaxLoc(difference.reshape(1), nullptr, &max_abs);
    const cv::Scalar channel_mean = cv::mean(difference);
    const double mean_abs = (channel_mean[0] + channel_mean[1] + channel_mean[2]) / 3.0;
    const bool passed = mean_abs <= kDisplayMeanAbsThreshold && max_abs <= kDisplayMaxAbsThreshold;
    return {{"mean_abs", mean_abs}, {"max_abs", max_abs},
        {"mean_abs_threshold", kDisplayMeanAbsThreshold},
        {"max_abs_threshold", kDisplayMaxAbsThreshold}, {"passed", passed}};
}

void require_nonblank(const rt::RadianceFrame& frame, const cv::Mat& linear) {
    cv::Scalar mean;
    cv::Scalar deviation;
    cv::meanStdDev(linear, mean, deviation);
    const double max_deviation = std::max({deviation[0], deviation[1], deviation[2]});
    if (!std::isfinite(frame.average_luminance) || frame.average_luminance <= 0.01
        || max_deviation <= 0.005) {
        throw std::runtime_error("acceptance render is blank or lacks scene variation");
    }
}

ArtifactPair write_and_compare(const ViewSpec& view, const rt::RadianceFrame& frame,
    const std::filesystem::path& output_dir, const std::filesystem::path& reference_dir,
    bool approve_references) {
    ArtifactPair artifacts {
        .exr = output_dir / (view.id + ".exr"),
        .png = output_dir / (view.id + ".png"),
    };
    const cv::Mat linear = linear_bgr(frame);
    const cv::Mat display = display_bgr(frame);
    require_nonblank(frame, linear);
    write_linear_exr(artifacts.exr, linear);
    if (!cv::imwrite(artifacts.png.string(), display)) {
        throw std::runtime_error("failed to write acceptance images for " + view.id);
    }

    const std::filesystem::path reference_exr = reference_dir / artifacts.exr.filename();
    const std::filesystem::path reference_png = reference_dir / artifacts.png.filename();
    if (approve_references) {
        std::filesystem::create_directories(reference_dir);
        std::filesystem::copy_file(artifacts.exr, reference_exr,
            std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(artifacts.png, reference_png,
            std::filesystem::copy_options::overwrite_existing);
    }
    const cv::Mat expected_linear = read_linear_exr(reference_exr);
    const cv::Mat expected_display = cv::imread(reference_png.string(), cv::IMREAD_UNCHANGED);
    if (expected_linear.empty() || expected_display.empty()) {
        throw std::runtime_error("approved references are missing for " + view.id);
    }
    const json linear_metrics = compare_linear(linear, expected_linear);
    const json display_metrics = compare_display(display, expected_display);
    if (!linear_metrics.at("passed").get<bool>() || !display_metrics.at("passed").get<bool>()) {
        throw std::runtime_error("approved reference gate failed for " + view.id);
    }

    artifacts.output = {{"view_id", view.id}, {"linear_exr", artifacts.exr.filename().string()},
        {"linear_exr_sha256", sha256(artifacts.exr)},
        {"display_png", artifacts.png.filename().string()},
        {"display_png_sha256", sha256(artifacts.png)},
        {"average_luminance", frame.average_luminance}};
    artifacts.reference = {{"view_id", view.id}, {"linear", linear_metrics},
        {"perceptual", display_metrics}, {"reference_exr_sha256", sha256(reference_exr)},
        {"reference_png_sha256", sha256(reference_png)}};
    return artifacts;
}

std::vector<ViewSpec> single_views() {
    std::vector<ViewSpec> result;
    const std::array<std::tuple<std::string_view, double, double>, 3> poses {{
        {"front_three_quarter", 45.0, 0.25},
        {"rear_three_quarter", 225.0, 0.25},
        {"elevated_three_quarter", 135.0, 0.85},
    }};
    for (const auto& [pose, azimuth, elevation] : poses) {
        for (const rt::CameraModelType model :
            {rt::CameraModelType::pinhole32, rt::CameraModelType::equi62_lut1d}) {
            result.push_back(ViewSpec {
                .id = std::string {pose} + "__" + model_name(model),
                .pose = std::string {pose},
                .model = model,
                .azimuth_degrees = azimuth,
                .elevation_ratio = elevation,
            });
        }
    }
    return result;
}

std::vector<ViewSpec> orbit_views() {
    const std::array<double, 4> azimuths {45.0, 135.0, 225.0, 315.0};
    std::vector<ViewSpec> result;
    for (int i = 0; i < 4; ++i) {
        const rt::CameraModelType model =
            i % 2 == 0 ? rt::CameraModelType::pinhole32 : rt::CameraModelType::equi62_lut1d;
        result.push_back(ViewSpec {
            .id = "orbit_4_mixed_models__cam" + std::to_string(i) + "__" + model_name(model),
            .pose = "orbit_4_mixed_models",
            .model = model,
            .azimuth_degrees = azimuths[static_cast<std::size_t>(i)],
            .elevation_ratio = 0.35,
            .camera_index = i,
            .simultaneous = true,
        });
    }
    return result;
}

void write_manifest(const std::filesystem::path& path, const json& manifest) {
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary);
    if (!output) {
        throw std::runtime_error("failed to open acceptance manifest: " + temporary.string());
    }
    output << std::setw(2) << manifest << '\n';
    output.close();
    std::filesystem::rename(temporary, path);
}

} // namespace

int main(int argc, const char* argv[]) {
    try {
        std::string stage_path;
        std::string openpbr_dir;
        std::string output_dir;
        std::string reference_dir;
        bool approve_references = false;
        argparse::ArgumentParser program("render_public_acceptance");
        program.add_argument("--stage").required().store_into(stage_path);
        program.add_argument("--openpbr-dir").required().store_into(openpbr_dir);
        program.add_argument("--output-dir").required().store_into(output_dir);
        program.add_argument("--reference-dir").required().store_into(reference_dir);
        program.add_argument("--approve-references")
            .default_value(false)
            .implicit_value(true)
            .store_into(approve_references);
        program.parse_args(argc, argv);

        const std::filesystem::path output_path = output_dir;
        const std::filesystem::path reference_path = reference_dir;
        std::filesystem::create_directories(output_path);

        const auto openpbr_documents = rt::scene::load_materialx_openpbr_directory(openpbr_dir);
        for (const auto& document : openpbr_documents) {
            static_cast<void>(rt::scene::compile_openpbr_core_material(document.surface));
        }
        if (openpbr_documents.size() != 83) {
            throw std::runtime_error("pinned OpenPBR corpus must compile exactly 83 examples");
        }

        const rt::scene::SceneIRv2 scene_ir = rt::scene::import_openusd_stage(stage_path);
        rt::SceneDescription description = rt::scene::adapt_scene_ir_v2_to_realtime(scene_ir);
        description.background = Eigen::Vector3d {0.65, 0.75, 1.0};
        const rt::PackedScene scene = description.pack();
        const Bounds bounds = scene_bounds(scene);

        rt::RenderProfile profile = rt::RenderProfile::realtime();
        profile.samples_per_pixel = 16;
        profile.max_bounces = 4;
        profile.enable_denoise = false;
        profile.rr_start_bounce = 3;

        rt::RendererPool renderers(4);
        renderers.prepare_scene(scene);
        json cameras = json::array();
        json outputs = json::array();
        json references = json::array();

        std::uint32_t stream = kSampleSeed;
        for (const ViewSpec& view : single_views()) {
            rt::PackedCameraRig rig;
            rig.active_count = 1;
            rig.cameras[0] = make_camera(view, bounds, scene_ir.stage_metadata().up_axis);
            rig.cameras[0].enabled = 1;
            renderers.reset_accumulation();
            renderers.reset_sequence(stream++);
            const auto result = renderers.render_frame(rig, profile, 1);
            const ArtifactPair artifacts = write_and_compare(view, result.at(0).profiled.frame,
                output_path, reference_path, approve_references);
            cameras.push_back(camera_json(view, rig.cameras[0]));
            outputs.push_back(artifacts.output);
            references.push_back(artifacts.reference);
            std::cout << "rendered " << view.id << '\n';
        }

        const std::vector<ViewSpec> orbit = orbit_views();
        rt::PackedCameraRig orbit_rig;
        orbit_rig.active_count = 4;
        for (const ViewSpec& view : orbit) {
            orbit_rig.cameras[static_cast<std::size_t>(view.camera_index)] =
                make_camera(view, bounds, scene_ir.stage_metadata().up_axis);
            orbit_rig.cameras[static_cast<std::size_t>(view.camera_index)].enabled = 1;
        }
        const std::string simultaneous_submission_id =
            "orbit_4_mixed_models-seed-" + std::to_string(stream);
        renderers.reset_accumulation();
        renderers.reset_sequence(stream);
        const auto orbit_results = renderers.render_frame(orbit_rig, profile, 4);
        for (const rt::CameraRenderResult& result : orbit_results) {
            const ViewSpec& view = orbit.at(static_cast<std::size_t>(result.camera_index));
            const ArtifactPair artifacts = write_and_compare(view, result.profiled.frame,
                output_path, reference_path, approve_references);
            cameras.push_back(camera_json(view,
                orbit_rig.cameras[static_cast<std::size_t>(result.camera_index)]));
            outputs.push_back(artifacts.output);
            references.push_back(artifacts.reference);
            std::cout << "rendered simultaneous " << view.id << '\n';
        }

        if (outputs.size() != 10) {
            throw std::runtime_error("acceptance matrix did not emit exactly ten view outputs");
        }
        const json manifest = {
            {"schema", "public_acceptance_render_outputs_v1"},
            {"source_revisions",
                {{"usd_repository", "usd-wg/assets"}, {"usd_revision", RT_ACCEPTANCE_USD_REVISION},
                    {"usd_stage", RT_ACCEPTANCE_USD_ENTRYPOINT},
                    {"openpbr_repository", "AcademySoftwareFoundation/OpenPBR"},
                    {"openpbr_revision", RT_ACCEPTANCE_OPENPBR_REVISION},
                    {"openpbr_examples", openpbr_documents.size()}}},
            {"render_settings",
                {{"width", kWidth}, {"height", kHeight},
                    {"samples_per_pixel", profile.samples_per_pixel},
                    {"max_bounces", profile.max_bounces}, {"denoise", profile.enable_denoise},
                    {"environment_linear_rgb", {0.65, 0.75, 1.0}},
                    {"formats", {"scene_linear_exr", "display_png"}}}},
            {"sample_seed", kSampleSeed},
            {"scene_bounds", {{"min", {bounds.min.x(), bounds.min.y(), bounds.min.z()}},
                                 {"max", {bounds.max.x(), bounds.max.y(), bounds.max.z()}}}},
            {"scene_stats",
                {{"triangles", scene.triangle_count}, {"materials", scene.material_count},
                    {"textures", scene.texture_count}}},
            {"cameras", cameras},
            {"outputs", outputs},
            {"simultaneous_submission_id", simultaneous_submission_id},
            {"reference_metrics", references},
            {"reference_gate_passed", true},
        };
        write_manifest(output_path / "manifest.json", manifest);
        if (approve_references) {
            json reference_manifest = manifest;
            reference_manifest["schema"] = "public_acceptance_reference_bundle_v1";
            reference_manifest["approved"] = true;
            write_manifest(reference_path / "manifest.json", reference_manifest);
        }
        std::cout << "acceptance passed: 10 views, 20 images, manifest="
                  << (output_path / "manifest.json") << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "public render acceptance failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
