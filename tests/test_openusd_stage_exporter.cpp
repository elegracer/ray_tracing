#include "scene/openusd_stage_exporter.h"
#include "scene/openusd_stage_importer.h"
#include "test_support.h"

#include <Eigen/Core>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unistd.h>
#include <variant>

namespace {

constexpr double kFloatTolerance = 1e-6;

struct TemporaryDirectory {
    std::filesystem::path path;

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream {path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error("failed to read exported OpenUSD stage: " + path.string());
    }
    return {std::istreambuf_iterator<char> {stream}, std::istreambuf_iterator<char> {}};
}

void expect_matrix_near(const Eigen::Matrix4d& actual, const Eigen::Matrix4d& expected,
    double tolerance, const std::string& label) {
    expect_true((actual - expected).cwiseAbs().maxCoeff() <= tolerance, label);
}

void expect_metadata_equal(const rt::scene::SceneStageMetadata& actual,
    const rt::scene::SceneStageMetadata& expected) {
    expect_near(actual.meters_per_unit, expected.meters_per_unit, 1e-12, "meters per unit");
    expect_true(actual.up_axis == expected.up_axis, "up axis");
    expect_true(actual.right_handed == expected.right_handed, "stage handedness");
    expect_near(actual.time_codes_per_second, expected.time_codes_per_second, 1e-12,
        "time codes per second");
    expect_near(actual.frames_per_second, expected.frames_per_second, 1e-12, "frames per second");
    expect_true(actual.start_time_code == expected.start_time_code, "start time code");
    expect_true(actual.end_time_code == expected.end_time_code, "end time code");
    expect_true(actual.interpolation == expected.interpolation, "stage interpolation");
    expect_true(actual.default_prim_path == expected.default_prim_path, "default prim");
}

void expect_assets_equal(const std::vector<rt::scene::SceneAssetReference>& actual,
    const std::vector<rt::scene::SceneAssetReference>& expected, const std::string& path) {
    expect_true(actual.size() == expected.size(), path + " asset count");
    for (std::size_t index = 0; index < actual.size(); ++index) {
        expect_true(actual[index].authored_path == expected[index].authored_path,
            path + " authored asset");
        expect_true(actual[index].evaluated_path == expected[index].evaluated_path,
            path + " evaluated asset");
        expect_true(actual[index].resolved_path == expected[index].resolved_path,
            path + " resolved asset");
    }
}

void expect_geometry_equal(const rt::scene::SceneGeometry& actual,
    const rt::scene::SceneGeometry& expected, const std::string& path) {
    expect_true(actual.index() == expected.index(), path + " geometry type");
    std::visit(
        [&](const auto& actual_geometry) {
            using T = std::decay_t<decltype(actual_geometry)>;
            const T& expected_geometry = std::get<T>(expected);
            if constexpr (std::is_same_v<T, rt::scene::SceneSphereGeometry>) {
                expect_vec3_near(actual_geometry.center, expected_geometry.center, kFloatTolerance,
                    path + " sphere center");
                expect_near(actual_geometry.radius, expected_geometry.radius, kFloatTolerance,
                    path + " sphere radius");
            } else {
                expect_true(actual_geometry.points.size() == expected_geometry.points.size(),
                    path + " mesh point count");
                for (std::size_t index = 0; index < actual_geometry.points.size(); ++index) {
                    expect_vec3_near(actual_geometry.points[index], expected_geometry.points[index],
                        kFloatTolerance, path + " mesh point");
                }
                expect_true(actual_geometry.face_vertex_counts
                                == expected_geometry.face_vertex_counts,
                    path + " mesh face counts");
                expect_true(actual_geometry.face_vertex_indices
                                == expected_geometry.face_vertex_indices,
                    path + " mesh face indices");
                expect_true(actual_geometry.orientation == expected_geometry.orientation,
                    path + " mesh orientation");
                expect_true(actual_geometry.subdivision_scheme
                                == expected_geometry.subdivision_scheme,
                    path + " mesh subdivision");
                expect_true(actual_geometry.primvars.empty() && expected_geometry.primvars.empty(),
                    path + " curated mesh primvars");
                expect_true(actual_geometry.material_subsets.empty()
                                && expected_geometry.material_subsets.empty(),
                    path + " curated material subsets");
            }
        },
        actual);
}

void expect_camera_equal(const rt::scene::SceneCamera& actual,
    const rt::scene::SceneCamera& expected, const std::string& path) {
    expect_true(actual.projection == expected.projection, path + " camera projection");
    expect_near(actual.horizontal_aperture, expected.horizontal_aperture, kFloatTolerance,
        path + " horizontal aperture");
    expect_near(actual.vertical_aperture, expected.vertical_aperture, kFloatTolerance,
        path + " vertical aperture");
    expect_near(actual.horizontal_aperture_offset, expected.horizontal_aperture_offset,
        kFloatTolerance, path + " horizontal aperture offset");
    expect_near(actual.vertical_aperture_offset, expected.vertical_aperture_offset, kFloatTolerance,
        path + " vertical aperture offset");
    expect_near(actual.focal_length, expected.focal_length, kFloatTolerance,
        path + " focal length");
    expect_true((actual.clipping_range - expected.clipping_range).cwiseAbs().maxCoeff()
                    <= kFloatTolerance,
        path + " clipping range");
    expect_near(actual.f_stop, expected.f_stop, kFloatTolerance, path + " f-stop");
    expect_near(actual.focus_distance, expected.focus_distance, kFloatTolerance,
        path + " focus distance");
    expect_true(!actual.renderer_calibration && !expected.renderer_calibration,
        path + " curated camera calibration");
}

void expect_light_equal(const rt::scene::SceneLight& actual, const rt::scene::SceneLight& expected,
    const std::string& path) {
    expect_true(actual.type == expected.type, path + " light type");
    expect_vec3_near(actual.color, expected.color, kFloatTolerance, path + " light color");
    expect_near(actual.intensity, expected.intensity, kFloatTolerance, path + " intensity");
    expect_near(actual.exposure, expected.exposure, kFloatTolerance, path + " exposure");
    expect_true(actual.normalize == expected.normalize, path + " normalize");
    expect_true(actual.enable_color_temperature == expected.enable_color_temperature,
        path + " color temperature enable");
    expect_near(actual.color_temperature_kelvin, expected.color_temperature_kelvin, kFloatTolerance,
        path + " color temperature");
    expect_near(actual.diffuse, expected.diffuse, kFloatTolerance, path + " diffuse");
    expect_near(actual.specular, expected.specular, kFloatTolerance, path + " specular");
    expect_true(actual.material_sync_mode == expected.material_sync_mode,
        path + " material sync mode");
    expect_near(actual.radius, expected.radius, kFloatTolerance, path + " radius");
    expect_near(actual.width, expected.width, kFloatTolerance, path + " width");
    expect_near(actual.height, expected.height, kFloatTolerance, path + " height");
    expect_near(actual.length, expected.length, kFloatTolerance, path + " length");
    expect_near(actual.angle_degrees, expected.angle_degrees, kFloatTolerance, path + " angle");
    expect_true(actual.treat_as_point == expected.treat_as_point, path + " treat as point");
    expect_true(actual.treat_as_line == expected.treat_as_line, path + " treat as line");
}

void expect_openpbr_equal(const rt::scene::SceneOpenPbrSurface& actual,
    const rt::scene::SceneOpenPbrSurface& expected, const std::string& path) {
    using ScalarMember = double rt::scene::SceneOpenPbrSurface::*;
    static const std::array<ScalarMember, 27> scalar_members {{
        &rt::scene::SceneOpenPbrSurface::base_weight,
        &rt::scene::SceneOpenPbrSurface::base_diffuse_roughness,
        &rt::scene::SceneOpenPbrSurface::base_metalness,
        &rt::scene::SceneOpenPbrSurface::specular_weight,
        &rt::scene::SceneOpenPbrSurface::specular_roughness,
        &rt::scene::SceneOpenPbrSurface::specular_ior,
        &rt::scene::SceneOpenPbrSurface::specular_roughness_anisotropy,
        &rt::scene::SceneOpenPbrSurface::transmission_weight,
        &rt::scene::SceneOpenPbrSurface::transmission_depth,
        &rt::scene::SceneOpenPbrSurface::transmission_scatter_anisotropy,
        &rt::scene::SceneOpenPbrSurface::transmission_dispersion_scale,
        &rt::scene::SceneOpenPbrSurface::transmission_dispersion_abbe_number,
        &rt::scene::SceneOpenPbrSurface::subsurface_weight,
        &rt::scene::SceneOpenPbrSurface::subsurface_radius,
        &rt::scene::SceneOpenPbrSurface::subsurface_scatter_anisotropy,
        &rt::scene::SceneOpenPbrSurface::fuzz_weight,
        &rt::scene::SceneOpenPbrSurface::fuzz_roughness,
        &rt::scene::SceneOpenPbrSurface::coat_weight,
        &rt::scene::SceneOpenPbrSurface::coat_roughness,
        &rt::scene::SceneOpenPbrSurface::coat_roughness_anisotropy,
        &rt::scene::SceneOpenPbrSurface::coat_ior,
        &rt::scene::SceneOpenPbrSurface::coat_darkening,
        &rt::scene::SceneOpenPbrSurface::thin_film_weight,
        &rt::scene::SceneOpenPbrSurface::thin_film_thickness,
        &rt::scene::SceneOpenPbrSurface::thin_film_ior,
        &rt::scene::SceneOpenPbrSurface::emission_luminance,
        &rt::scene::SceneOpenPbrSurface::geometry_opacity,
    }};
    for (const ScalarMember member : scalar_members) {
        expect_near(actual.*member, expected.*member, kFloatTolerance, path + " OpenPBR scalar");
    }

    using ColorMember = Eigen::Vector3d rt::scene::SceneOpenPbrSurface::*;
    static const std::array<ColorMember, 9> color_members {{
        &rt::scene::SceneOpenPbrSurface::base_color,
        &rt::scene::SceneOpenPbrSurface::specular_color,
        &rt::scene::SceneOpenPbrSurface::transmission_color,
        &rt::scene::SceneOpenPbrSurface::transmission_scatter,
        &rt::scene::SceneOpenPbrSurface::subsurface_color,
        &rt::scene::SceneOpenPbrSurface::subsurface_radius_scale,
        &rt::scene::SceneOpenPbrSurface::fuzz_color,
        &rt::scene::SceneOpenPbrSurface::coat_color,
        &rt::scene::SceneOpenPbrSurface::emission_color,
    }};
    for (const ColorMember member : color_members) {
        expect_vec3_near(actual.*member, expected.*member, kFloatTolerance,
            path + " OpenPBR color");
    }
    expect_true(actual.version == expected.version, path + " OpenPBR version");
    expect_true(actual.geometry_thin_walled == expected.geometry_thin_walled,
        path + " thin-walled");
    expect_true(actual.geometry_normal_default_geomprop
                    == expected.geometry_normal_default_geomprop,
        path + " normal geomprop");
    expect_true(actual.geometry_coat_normal_default_geomprop
                    == expected.geometry_coat_normal_default_geomprop,
        path + " coat normal geomprop");
    expect_true(actual.geometry_tangent_default_geomprop
                    == expected.geometry_tangent_default_geomprop,
        path + " tangent geomprop");
    expect_true(actual.geometry_coat_tangent_default_geomprop
                    == expected.geometry_coat_tangent_default_geomprop,
        path + " coat tangent geomprop");
    expect_true(actual.connections.empty() && expected.connections.empty(),
        path + " curated OpenPBR connections");
    expect_true(actual.displacement.type == expected.displacement.type,
        path + " curated displacement");
    expect_true(actual.energy_policy == expected.energy_policy, path + " energy policy");
}

void expect_prim_equal(const rt::scene::ScenePrim& actual, const rt::scene::ScenePrim& expected) {
    const std::string& path = expected.path;
    expect_true(actual.path == path, path + " stable identity");
    expect_true(actual.kind == expected.kind, path + " prim kind");
    expect_matrix_near(actual.local_to_parent, expected.local_to_parent, 1e-12,
        path + " local transform");
    expect_true(actual.reset_xform_stack == expected.reset_xform_stack,
        path + " reset xform stack");
    expect_true(actual.visibility == expected.visibility, path + " visibility");
    expect_true(actual.authored_purpose == expected.authored_purpose, path + " purpose");
    expect_true(actual.transform_samples.size() == expected.transform_samples.size(),
        path + " transform sample count");
    for (std::size_t index = 0; index < actual.transform_samples.size(); ++index) {
        expect_near(actual.transform_samples[index].time_code,
            expected.transform_samples[index].time_code, 1e-12, path + " sample time");
        expect_matrix_near(actual.transform_samples[index].local_to_parent,
            expected.transform_samples[index].local_to_parent, 1e-12, path + " sampled transform");
    }
    expect_true(actual.prototype_path == expected.prototype_path, path + " prototype binding");
    expect_true(actual.material_path == expected.material_path, path + " material binding");
    expect_true(actual.volume_density == expected.volume_density, path + " volume density");
    expect_assets_equal(actual.asset_references, expected.asset_references, path);

    expect_true(actual.geometry.has_value() == expected.geometry.has_value(), path + " geometry");
    if (actual.geometry) {
        expect_geometry_equal(*actual.geometry, *expected.geometry, path);
    }
    expect_true(actual.camera.has_value() == expected.camera.has_value(), path + " camera");
    if (actual.camera) {
        expect_camera_equal(*actual.camera, *expected.camera, path);
    }
    expect_true(actual.light.has_value() == expected.light.has_value(), path + " light");
    if (actual.light) {
        expect_light_equal(*actual.light, *expected.light, path);
    }
    expect_true(actual.material.has_value() == expected.material.has_value(), path + " material");
    if (actual.material) {
        expect_true(actual.material->index() == expected.material->index(),
            path + " material type");
        expect_openpbr_equal(std::get<rt::scene::SceneOpenPbrSurface>(*actual.material),
            std::get<rt::scene::SceneOpenPbrSurface>(*expected.material), path);
    }
}

void expect_scene_equal(const rt::scene::SceneIRv2& actual, const rt::scene::SceneIRv2& expected) {
    expect_metadata_equal(actual.stage_metadata(), expected.stage_metadata());
    expect_true(actual.prims().size() == expected.prims().size(), "round-trip prim count");
    for (const rt::scene::ScenePrim& expected_prim : expected.prims()) {
        const rt::scene::ScenePrim* actual_prim = actual.find_prim(expected_prim.path);
        expect_true(actual_prim != nullptr, "round-trip prim path " + expected_prim.path);
        expect_prim_equal(*actual_prim, expected_prim);
    }
}

} // namespace

int main(int argc, char** argv) {
    expect_true(argc == 2, "OpenUSD exporter test requires one fixture path");
    const std::filesystem::path fixture = argv[1];

#if RT_HAS_OPENUSD
    expect_true(rt::scene::openusd_stage_exporter_available(), "OpenUSD exporter capability");
    const rt::scene::SceneIRv2 source = rt::scene::import_openusd_stage(fixture);

    TemporaryDirectory temporary {
        std::filesystem::temp_directory_path()
        / ("ray_tracing_openusd_roundtrip_" + std::to_string(::getpid()))};
    std::filesystem::remove_all(temporary.path);
    std::filesystem::create_directories(temporary.path);
    const std::filesystem::path first_path = temporary.path / "first.usda";
    const std::filesystem::path second_path = temporary.path / "second.usda";
    const std::filesystem::path canonical_path = temporary.path / "canonical.usda";

    rt::scene::export_openusd_stage(source, first_path);
    rt::scene::export_openusd_stage(source, second_path);
    expect_true(read_file(first_path) == read_file(second_path),
        "independent exports are byte deterministic");

    const rt::scene::SceneIRv2 round_trip = rt::scene::import_openusd_stage(first_path);
    expect_scene_equal(round_trip, source);
    rt::scene::export_openusd_stage(round_trip, canonical_path);
    expect_true(read_file(first_path) == read_file(canonical_path),
        "import-export-import canonical bytes remain stable");

    try {
        rt::scene::export_openusd_stage(source, temporary.path / "unsupported.usd");
    } catch (const std::invalid_argument& error) {
        expect_true(std::string {error.what()}.find("requires a .usda destination")
                        != std::string::npos,
            "binary-container determinism boundary is explicit");
        return 0;
    }
    throw std::runtime_error("deterministic exporter accepted a non-USDA destination");
#else
    expect_true(!rt::scene::openusd_stage_exporter_available(), "disabled exporter capability");
    try {
        rt::scene::export_openusd_stage({}, fixture);
    } catch (const std::runtime_error& error) {
        expect_true(std::string {error.what()}.find("RT_ENABLE_OPENUSD=ON") != std::string::npos,
            "disabled exporter reports the exact build capability");
        return 0;
    }
    throw std::runtime_error("disabled OpenUSD exporter unexpectedly accepted a scene");
#endif
}
