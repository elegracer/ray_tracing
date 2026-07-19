#pragma once

#include "scene/shared_scene_ir.h"

#include <Eigen/Core>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace rt::scene {

enum class SceneUpAxis {
    y,
    z,
};

enum class SceneTimeInterpolation {
    held,
    linear,
};

enum class SceneVisibility {
    inherited,
    invisible,
};

enum class ScenePurpose {
    default_,
    render,
    proxy,
    guide,
};

enum class ScenePrimKind {
    scope,
    texture,
    material,
    geometry_prototype,
    surface,
    volume,
    camera,
    light,
};

enum class SceneCameraProjection {
    perspective,
    orthographic,
};

enum class SceneCameraCalibrationModel {
    pinhole32,
    equi62_lut1d,
};

struct SceneCameraCalibration {
    SceneCameraCalibrationModel model = SceneCameraCalibrationModel::pinhole32;
    int image_width = 0;
    int image_height = 0;
    Eigen::Vector2d focal_length_pixels = Eigen::Vector2d::Zero();
    Eigen::Vector2d principal_point_pixels = Eigen::Vector2d::Zero();
    std::vector<double> radial_distortion;
    Eigen::Vector2d tangential_distortion = Eigen::Vector2d::Zero();
    Eigen::Matrix4d camera_to_body = Eigen::Matrix4d::Identity();
    bool compatibility_pose_fallback = false;
};

struct SceneCamera {
    SceneCameraProjection projection = SceneCameraProjection::perspective;
    // OpenUSD filmback and focal values are measured in tenths of a scene unit.
    double horizontal_aperture = 20.955;
    double vertical_aperture = 15.2908;
    double horizontal_aperture_offset = 0.0;
    double vertical_aperture_offset = 0.0;
    double focal_length = 50.0;
    Eigen::Vector2d clipping_range = Eigen::Vector2d {1.0, 1'000'000.0};
    double f_stop = 0.0;
    double focus_distance = 0.0;
    std::optional<SceneCameraCalibration> renderer_calibration;
};

struct SceneAssetReference {
    std::string authored_path;
    std::string evaluated_path;
    std::string resolved_path;
};

enum class SceneLightType {
    geometry,
    sphere,
    disk,
    rect,
    cylinder,
    distant,
    dome,
};

enum class SceneLightMaterialSyncMode {
    material_glow_tints_light,
    independent,
    no_material_response,
};

enum class SceneLightIntensityUnit {
    nit,
    lux,
};

struct SceneLight {
    SceneLightType type = SceneLightType::sphere;
    // UsdLux inputs:color is expressed in the rendering color space.
    Eigen::Vector3d color = Eigen::Vector3d::Ones();
    // Base luminance at exposure zero, in nits before normalization. A normalized
    // distant light interprets this value as illuminance in lux.
    double intensity = 1.0;
    // Exposure is measured in stops and multiplies intensity by exp2(exposure).
    double exposure = 0.0;
    bool normalize = false;
    bool enable_color_temperature = false;
    double color_temperature_kelvin = 6500.0;
    double diffuse = 1.0;
    double specular = 1.0;
    SceneLightMaterialSyncMode material_sync_mode =
        SceneLightMaterialSyncMode::no_material_response;

    // Shape parameters use authored scene units except for the distant-light angle.
    double radius = 0.5;
    double width = 1.0;
    double height = 1.0;
    double length = 1.0;
    // UsdLux clamps half this angle to [0, pi] when deriving distant normalization.
    double angle_degrees = 0.53;
    bool treat_as_point = false;
    bool treat_as_line = false;
};

enum class SceneMeshOrientation {
    right_handed,
    left_handed,
};

enum class SceneSubdivisionScheme {
    none,
    catmull_clark,
    loop,
    bilinear,
};

enum class ScenePrimvarInterpolation {
    constant,
    uniform,
    varying,
    vertex,
    face_varying,
};

enum class ScenePrimvarRole {
    none,
    point,
    normal,
    vector,
    color,
    texcoord,
};

using ScenePrimvarValues = std::variant<std::vector<std::int32_t>, std::vector<float>,
    std::vector<double>, std::vector<Eigen::Vector2f>, std::vector<Eigen::Vector3f>,
    std::vector<Eigen::Vector4f>, std::vector<Eigen::Vector2d>, std::vector<Eigen::Vector3d>,
    std::vector<Eigen::Vector4d>, std::vector<std::string>>;

struct ScenePrimvar {
    // Store the primvar base name (for example "st"), without the "primvars:" prefix.
    std::string name;
    ScenePrimvarInterpolation interpolation = ScenePrimvarInterpolation::constant;
    ScenePrimvarRole role = ScenePrimvarRole::none;
    std::size_t element_size = 1;
    ScenePrimvarValues values;
    std::vector<std::int32_t> indices;
};

enum class SceneMaterialSubsetFamilyType {
    non_overlapping,
    partition,
};

struct SceneMaterialSubset {
    std::string name;
    std::vector<std::int32_t> face_indices;
    std::string material_path;
};

struct SceneSphereGeometry {
    Eigen::Vector3d center = Eigen::Vector3d::Zero();
    double radius = 1.0;
};

struct SceneMeshGeometry {
    std::vector<Eigen::Vector3d> points;
    std::vector<std::int32_t> face_vertex_counts;
    std::vector<std::int32_t> face_vertex_indices;
    SceneMeshOrientation orientation = SceneMeshOrientation::right_handed;
    SceneSubdivisionScheme subdivision_scheme = SceneSubdivisionScheme::none;
    std::vector<ScenePrimvar> primvars;
    SceneMaterialSubsetFamilyType material_subset_family_type =
        SceneMaterialSubsetFamilyType::non_overlapping;
    std::vector<SceneMaterialSubset> material_subsets;
};

using SceneGeometry = std::variant<SceneSphereGeometry, SceneMeshGeometry>;

struct SceneStageMetadata {
    double meters_per_unit = 0.01;
    SceneUpAxis up_axis = SceneUpAxis::y;
    bool right_handed = true;
    double time_codes_per_second = 24.0;
    double frames_per_second = 24.0;
    std::optional<double> start_time_code;
    std::optional<double> end_time_code;
    SceneTimeInterpolation interpolation = SceneTimeInterpolation::linear;
    std::string default_prim_path = "/World";
};

struct SceneTransformSample {
    double time_code = 0.0;
    // Samples are canonical evaluated local-to-parent matrices; linear mode interpolates their components.
    Eigen::Matrix4d local_to_parent = Eigen::Matrix4d::Identity();
};

struct ScenePrim {
    std::string path;
    ScenePrimKind kind = ScenePrimKind::scope;
    Eigen::Matrix4d local_to_parent = Eigen::Matrix4d::Identity();
    bool reset_xform_stack = false;
    SceneVisibility visibility = SceneVisibility::inherited;
    std::optional<ScenePurpose> authored_purpose;
    std::vector<SceneTransformSample> transform_samples;

    std::string prototype_path;
    std::string material_path;
    std::optional<double> volume_density;
    std::optional<SceneGeometry> geometry;
    std::optional<SceneCamera> camera;
    // Intrinsic lights use kind=light. Geometry lights apply this payload to a surface prim.
    std::optional<SceneLight> light;
    std::vector<SceneAssetReference> asset_references;
    // Populated only by compatibility frontends; native v2 prims do not need an integer identity.
    std::optional<std::size_t> compatibility_source_index;
    std::optional<std::string> compatibility_source_name;
};

class SceneIRv2 {
public:
    SceneStageMetadata& stage_metadata();
    const SceneStageMetadata& stage_metadata() const;

    std::size_t add_prim(ScenePrim prim);
    const std::vector<ScenePrim>& prims() const;
    const ScenePrim* find_prim(std::string_view path) const;

private:
    SceneStageMetadata stage_metadata_;
    std::vector<ScenePrim> prims_;
    std::unordered_map<std::string, std::size_t> first_prim_by_path_;
};

enum class SceneDiagnosticSeverity {
    warning,
    error,
};

struct SceneDiagnostic {
    SceneDiagnosticSeverity severity = SceneDiagnosticSeverity::error;
    std::string code;
    std::string prim_path;
    std::string message;
};

struct SceneBackendCapabilities {
    std::string backend_name = "backend";
    bool full_affine_transforms = false;
    bool transform_time_samples = false;
    bool reset_xform_stack = false;
    bool non_render_purposes = false;
    bool mesh_ngons = false;
    bool mesh_primvars = false;
    bool subdivision_surfaces = false;
    bool material_subsets = false;
    bool orthographic_cameras = false;
    bool camera_model_extensions = false;
    bool camera_distortion = false;
    bool asset_references = false;
    // usd_lux_lights implies support for every common LightAPI input, including exposure.
    bool usd_lux_lights = false;
    bool analytic_lights = false;
    bool dome_lights = false;
    bool geometry_lights = false;
    bool light_normalization = false;
    bool light_color_temperature = false;
};

bool is_valid_scene_prim_path(std::string_view path);
std::string parent_scene_prim_path(std::string_view path);

std::vector<SceneDiagnostic> validate_scene_ir_v2(const SceneIRv2& scene);
void require_valid_scene_ir_v2(const SceneIRv2& scene);
bool has_scene_diagnostic(const std::vector<SceneDiagnostic>& diagnostics, std::string_view code);

std::vector<SceneDiagnostic> diagnose_scene_ir_v2_capabilities(const SceneIRv2& scene,
    const SceneBackendCapabilities& capabilities);

Eigen::Matrix4d sample_scene_local_transform(const ScenePrim& prim, double time_code,
    SceneTimeInterpolation interpolation);
Eigen::Matrix4d compute_scene_world_transform(const SceneIRv2& scene, std::string_view prim_path,
    double time_code);
bool compute_scene_visibility(const SceneIRv2& scene, std::string_view prim_path);
ScenePurpose compute_scene_purpose(const SceneIRv2& scene, std::string_view prim_path);

double scene_light_exposed_intensity(const SceneLight& light);
SceneLightIntensityUnit scene_light_intensity_unit(const SceneLight& light);

SceneIRv2 compile_legacy_scene_ir_v2(const SceneIR& legacy_scene);
SceneIRv2 compile_legacy_scene_ir_v2(const SceneIR& legacy_scene, SceneStageMetadata metadata);

} // namespace rt::scene
