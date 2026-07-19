#include "scene/scene_ir_v2.h"

#include "scene/scene_ir_validator.h"

#include <Eigen/LU>
#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace rt::scene {
namespace {

bool is_identifier(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    const auto is_alpha_or_underscore = [](unsigned char c) {
        return std::isalpha(c) != 0 || c == '_';
    };
    const auto is_alnum_or_underscore = [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '_';
    };
    if (!is_alpha_or_underscore(static_cast<unsigned char>(value.front()))) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(),
        [&](char c) { return is_alnum_or_underscore(static_cast<unsigned char>(c)); });
}

bool is_namespaced_identifier(std::string_view value) {
    if (value.empty() || value.front() == ':' || value.back() == ':') {
        return false;
    }
    std::size_t component_start = 0;
    while (component_start < value.size()) {
        const std::size_t separator = value.find(':', component_start);
        const std::size_t component_end =
            separator == std::string_view::npos ? value.size() : separator;
        if (!is_identifier(value.substr(component_start, component_end - component_start))) {
            return false;
        }
        if (separator == std::string_view::npos) {
            return true;
        }
        component_start = separator + 1;
    }
    return false;
}

bool is_finite_affine(const Eigen::Matrix4d& transform) {
    return transform.allFinite();
}

bool has_affine_last_row(const Eigen::Matrix4d& transform) {
    return std::abs(transform(3, 0)) <= 1e-12 && std::abs(transform(3, 1)) <= 1e-12
           && std::abs(transform(3, 2)) <= 1e-12 && std::abs(transform(3, 3) - 1.0) <= 1e-12;
}

bool is_singular_affine(const Eigen::Matrix4d& transform) {
    return std::abs(transform.block<3, 3>(0, 0).determinant()) <= 1e-12;
}

bool is_rigid_transform(const Eigen::Matrix4d& transform) {
    const Eigen::Matrix3d linear = transform.block<3, 3>(0, 0);
    return (linear.transpose() * linear).isApprox(Eigen::Matrix3d::Identity(), 1e-9)
           && std::abs(std::abs(linear.determinant()) - 1.0) <= 1e-9;
}

void append_transform_diagnostics(std::vector<SceneDiagnostic>& diagnostics,
    const Eigen::Matrix4d& transform, std::string_view path) {
    if (!is_finite_affine(transform)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "xform.non_finite",
            std::string {path}, "transform contains a non-finite value"});
        return;
    }
    if (!has_affine_last_row(transform)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "xform.non_affine",
            std::string {path}, "transform must have affine last row [0, 0, 0, 1]"});
        return;
    }
    if (is_singular_affine(transform)) {
        diagnostics.push_back(
            {SceneDiagnosticSeverity::warning, "xform.singular", std::string {path},
                "singular affine transform may be unsupported by rendering backends"});
    }
}

std::string indexed_path(std::string_view scope, std::string_view label, std::size_t index) {
    return fmt::format("{}/{}_{:04d}", scope, label, index);
}

Eigen::Matrix4d legacy_transform_matrix(const Transform& transform) {
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    matrix.block<3, 3>(0, 0) = transform.rotation;
    matrix.block<3, 1>(0, 3) = transform.translation;
    return matrix;
}

std::size_t primvar_value_count(const ScenePrimvar& primvar) {
    return std::visit([](const auto& values) { return values.size(); }, primvar.values);
}

bool primvar_values_are_finite(const ScenePrimvar& primvar) {
    return std::visit(
        [](const auto& values) {
            using Value = typename std::decay_t<decltype(values)>::value_type;
            if constexpr (std::is_floating_point_v<Value>) {
                return std::all_of(values.begin(), values.end(),
                    [](Value value) { return std::isfinite(value); });
            } else if constexpr (requires(const Value& value) { value.allFinite(); }) {
                return std::all_of(values.begin(), values.end(),
                    [](const Value& value) { return value.allFinite(); });
            } else {
                return true;
            }
        },
        primvar.values);
}

bool primvar_role_matches_values(const ScenePrimvar& primvar) {
    const bool vector2 = std::holds_alternative<std::vector<Eigen::Vector2f>>(primvar.values)
                         || std::holds_alternative<std::vector<Eigen::Vector2d>>(primvar.values);
    const bool vector3 = std::holds_alternative<std::vector<Eigen::Vector3f>>(primvar.values)
                         || std::holds_alternative<std::vector<Eigen::Vector3d>>(primvar.values);
    const bool vector4 = std::holds_alternative<std::vector<Eigen::Vector4f>>(primvar.values)
                         || std::holds_alternative<std::vector<Eigen::Vector4d>>(primvar.values);
    switch (primvar.role) {
        case ScenePrimvarRole::none: return true;
        case ScenePrimvarRole::point:
        case ScenePrimvarRole::normal:
        case ScenePrimvarRole::vector: return vector3;
        case ScenePrimvarRole::color: return vector3 || vector4;
        case ScenePrimvarRole::texcoord: return vector2;
    }
    return false;
}

std::size_t primvar_domain_size(const ScenePrimvar& primvar, const SceneMeshGeometry& mesh) {
    switch (primvar.interpolation) {
        case ScenePrimvarInterpolation::constant: return 1;
        case ScenePrimvarInterpolation::uniform: return mesh.face_vertex_counts.size();
        case ScenePrimvarInterpolation::varying:
        case ScenePrimvarInterpolation::vertex: return mesh.points.size();
        case ScenePrimvarInterpolation::face_varying: return mesh.face_vertex_indices.size();
    }
    return 0;
}

void append_primvar_diagnostics(std::vector<SceneDiagnostic>& diagnostics,
    const ScenePrimvar& primvar, const SceneMeshGeometry& mesh, std::string_view path) {
    if (!is_namespaced_identifier(primvar.name) || primvar.name.starts_with("primvars:")) {
        diagnostics.push_back(
            {SceneDiagnosticSeverity::error, "geometry.primvar.name", std::string {path},
                "primvar name must be a valid base name without the primvars: prefix"});
    }
    if (primvar.element_size == 0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.primvar.element_size",
            std::string {path}, "primvar element_size must be positive"});
        return;
    }

    const std::size_t value_count = primvar_value_count(primvar);
    if (value_count == 0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.primvar.values_empty",
            std::string {path}, "primvar values must not be empty"});
        return;
    }
    if (!primvar_values_are_finite(primvar)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.primvar.non_finite",
            std::string {path}, "numeric primvar values must be finite"});
    }
    if (!primvar_role_matches_values(primvar)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.primvar.role_type",
            std::string {path}, "primvar role is incompatible with its value type"});
    }
    if (primvar.name == "normals" && primvar.role != ScenePrimvarRole::normal) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.primvar.normals_role",
            std::string {path}, "normals primvar must use the normal role"});
    }
    if (primvar.name == "tangents" && primvar.role != ScenePrimvarRole::vector) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.primvar.tangents_role",
            std::string {path}, "tangents primvar must use the vector role"});
    }

    const std::size_t domain_size = primvar_domain_size(primvar, mesh);
    if (value_count % primvar.element_size != 0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.primvar.element_count",
            std::string {path}, "primvar value count must be divisible by element_size"});
        return;
    }
    const std::size_t tuple_count = value_count / primvar.element_size;
    if (primvar.indices.empty()) {
        if (tuple_count != domain_size) {
            diagnostics.push_back(
                {SceneDiagnosticSeverity::error, "geometry.primvar.domain_size", std::string {path},
                    "unindexed primvar value count does not match its interpolation domain"});
        }
        return;
    }
    if (primvar.indices.size() != domain_size) {
        diagnostics.push_back(
            {SceneDiagnosticSeverity::error, "geometry.primvar.index_count", std::string {path},
                "indexed primvar index count does not match its interpolation domain"});
    }
    if (std::any_of(primvar.indices.begin(), primvar.indices.end(), [&](std::int32_t index) {
            return index < 0 || static_cast<std::size_t>(index) >= tuple_count;
        })) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.primvar.index_range",
            std::string {path}, "primvar index is out of range"});
    }
}

void append_mesh_diagnostics(std::vector<SceneDiagnostic>& diagnostics,
    const SceneMeshGeometry& mesh, std::string_view path,
    const std::unordered_map<std::string, const ScenePrim*>& prims_by_path) {
    if (mesh.points.empty()) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.mesh.points_empty",
            std::string {path}, "mesh points must not be empty"});
    } else if (std::any_of(mesh.points.begin(), mesh.points.end(),
                   [](const Eigen::Vector3d& point) { return !point.allFinite(); })) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.mesh.points_non_finite",
            std::string {path}, "mesh points must be finite"});
    }

    std::size_t expected_index_count = 0;
    bool invalid_face_count = false;
    for (std::int32_t count : mesh.face_vertex_counts) {
        if (count < 3) {
            invalid_face_count = true;
            continue;
        }
        expected_index_count += static_cast<std::size_t>(count);
    }
    if (mesh.face_vertex_counts.empty() || invalid_face_count) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.mesh.face_counts",
            std::string {path}, "mesh faces must each contain at least three vertices"});
    }
    if (expected_index_count != mesh.face_vertex_indices.size()) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.mesh.topology_size",
            std::string {path}, "sum(face_vertex_counts) must equal face_vertex_indices size"});
    }
    if (std::any_of(mesh.face_vertex_indices.begin(), mesh.face_vertex_indices.end(),
            [&](std::int32_t index) {
                return index < 0 || static_cast<std::size_t>(index) >= mesh.points.size();
            })) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.mesh.vertex_index_range",
            std::string {path}, "mesh face-vertex index is out of range"});
    }
    if (mesh.subdivision_scheme == SceneSubdivisionScheme::loop
        && std::any_of(mesh.face_vertex_counts.begin(), mesh.face_vertex_counts.end(),
            [](std::int32_t count) { return count != 3; })) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.mesh.loop_non_triangle",
            std::string {path}, "Loop subdivision requires triangle topology"});
    }

    std::unordered_set<std::string> primvar_names;
    for (const ScenePrimvar& primvar : mesh.primvars) {
        if (!primvar_names.insert(primvar.name).second) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.primvar.duplicate",
                std::string {path}, "primvar names must be unique on a mesh"});
        }
        append_primvar_diagnostics(diagnostics, primvar, mesh, path);
        if (primvar.name == "normals" && mesh.subdivision_scheme != SceneSubdivisionScheme::none) {
            diagnostics.push_back({SceneDiagnosticSeverity::warning,
                "geometry.mesh.subdivision_authored_normals", std::string {path},
                "authored normals on a subdivision mesh may be ignored by OpenUSD renderers"});
        }
    }

    std::unordered_set<std::string> subset_names;
    std::unordered_set<std::int32_t> claimed_faces;
    for (const SceneMaterialSubset& subset : mesh.material_subsets) {
        if (!is_identifier(subset.name)) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.subset.name",
                std::string {path}, "material subset name must be a valid prim identifier"});
        } else if (!subset_names.insert(subset.name).second) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.subset.duplicate_name",
                std::string {path}, "material subset names must be unique"});
        }
        const auto material = prims_by_path.find(subset.material_path);
        if (material == prims_by_path.end() || material->second->kind != ScenePrimKind::material) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.subset.material",
                std::string {path}, "material subset path does not resolve to a material"});
        }
        if (subset.face_indices.empty()) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.subset.faces_empty",
                std::string {path}, "material subset must contain at least one face"});
        }
        std::unordered_set<std::int32_t> local_faces;
        for (std::int32_t face : subset.face_indices) {
            if (face < 0 || static_cast<std::size_t>(face) >= mesh.face_vertex_counts.size()) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.subset.face_range",
                    std::string {path}, "material subset face index is out of range"});
                continue;
            }
            if (!local_faces.insert(face).second) {
                diagnostics.push_back(
                    {SceneDiagnosticSeverity::error, "geometry.subset.duplicate_face",
                        std::string {path}, "material subset contains a duplicate face index"});
            }
            if (!claimed_faces.insert(face).second) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.subset.overlap",
                    std::string {path}, "materialBind subsets must not overlap"});
            }
        }
    }
    if (mesh.material_subset_family_type == SceneMaterialSubsetFamilyType::partition
        && claimed_faces.size() != mesh.face_vertex_counts.size()) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.subset.partition",
            std::string {path}, "partition material subsets must cover every face exactly once"});
    }
}

bool camera_has_distortion(const SceneCameraCalibration& calibration) {
    return std::any_of(calibration.radial_distortion.begin(), calibration.radial_distortion.end(),
               [](double coefficient) { return coefficient != 0.0; })
           || !calibration.tangential_distortion.isZero(0.0);
}

void append_camera_diagnostics(std::vector<SceneDiagnostic>& diagnostics, const SceneCamera& camera,
    std::string_view path) {
    const auto finite_positive = [](double value) {
        return std::isfinite(value) && value > 0.0;
    };
    if (!finite_positive(camera.horizontal_aperture) || !finite_positive(camera.vertical_aperture)
        || !finite_positive(camera.focal_length)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "camera.filmback",
            std::string {path}, "camera apertures and focal length must be finite and positive"});
    }
    if (!std::isfinite(camera.horizontal_aperture_offset)
        || !std::isfinite(camera.vertical_aperture_offset)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "camera.aperture_offset",
            std::string {path}, "camera aperture offsets must be finite"});
    }
    if (!finite_positive(camera.clipping_range.x()) || !finite_positive(camera.clipping_range.y())
        || camera.clipping_range.x() >= camera.clipping_range.y()) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "camera.clipping_range",
            std::string {path}, "camera clipping range must be finite, positive, and increasing"});
    }
    if (!std::isfinite(camera.f_stop) || camera.f_stop < 0.0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "camera.f_stop", std::string {path},
            "camera f-stop must be finite and non-negative"});
    }
    if (!std::isfinite(camera.focus_distance) || camera.focus_distance < 0.0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "camera.focus_distance",
            std::string {path}, "camera focus distance must be finite and non-negative"});
    }

    if (!camera.renderer_calibration) {
        return;
    }
    const SceneCameraCalibration& calibration = *camera.renderer_calibration;
    if (calibration.image_width <= 0 || calibration.image_height <= 0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "camera.calibration.resolution",
            std::string {path}, "camera calibration resolution must be positive"});
    }
    if (!calibration.focal_length_pixels.allFinite() || calibration.focal_length_pixels.x() <= 0.0
        || calibration.focal_length_pixels.y() <= 0.0
        || !calibration.principal_point_pixels.allFinite()) {
        diagnostics.push_back(
            {SceneDiagnosticSeverity::error, "camera.calibration.intrinsics", std::string {path},
                "camera pixel intrinsics must be finite with positive focal lengths"});
    }
    if (!calibration.tangential_distortion.allFinite()
        || std::any_of(calibration.radial_distortion.begin(), calibration.radial_distortion.end(),
            [](double coefficient) { return !std::isfinite(coefficient); })) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "camera.calibration.distortion",
            std::string {path}, "camera distortion coefficients must be finite"});
    }
    if (!is_finite_affine(calibration.camera_to_body)
        || !has_affine_last_row(calibration.camera_to_body)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "camera.calibration.extrinsic",
            std::string {path}, "camera-to-body calibration must be a finite affine transform"});
    }
}

bool asset_path_has_ascii_control(std::string_view path) {
    return std::any_of(path.begin(), path.end(), [](char value) {
        const unsigned char byte = static_cast<unsigned char>(value);
        return byte < 0x20 || byte == 0x7f;
    });
}

void append_asset_diagnostics(std::vector<SceneDiagnostic>& diagnostics,
    const SceneAssetReference& asset, std::string_view path) {
    if (asset.authored_path.empty()) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "asset.authored_path_empty",
            std::string {path}, "asset references must preserve a non-empty authored path"});
    }
    if (asset_path_has_ascii_control(asset.authored_path)
        || asset_path_has_ascii_control(asset.evaluated_path)
        || asset_path_has_ascii_control(asset.resolved_path)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "asset.path_control_character",
            std::string {path}, "asset paths must not contain ASCII control characters"});
    }
}

void append_light_diagnostics(std::vector<SceneDiagnostic>& diagnostics, const SceneLight& light,
    std::string_view path) {
    if (!light.color.allFinite() || (light.color.array() < 0.0).any()) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.color", std::string {path},
            "light color must be finite and non-negative in the rendering color space"});
    }
    if (!std::isfinite(light.intensity) || light.intensity < 0.0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.intensity",
            std::string {path}, "light intensity must be finite and non-negative"});
    }
    if (!std::isfinite(light.exposure)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.exposure", std::string {path},
            "light exposure must be finite"});
    }
    if (std::isfinite(light.intensity) && std::isfinite(light.exposure)
        && !std::isfinite(scene_light_exposed_intensity(light))) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.exposed_intensity",
            std::string {path}, "light intensity multiplied by exp2(exposure) must remain finite"});
    }
    if (!std::isfinite(light.color_temperature_kelvin) || light.color_temperature_kelvin < 1000.0
        || light.color_temperature_kelvin > 10000.0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.color_temperature",
            std::string {path}, "light color temperature must be between 1000 and 10000 kelvin"});
    }
    if (!std::isfinite(light.diffuse) || light.diffuse < 0.0 || !std::isfinite(light.specular)
        || light.specular < 0.0) {
        diagnostics.push_back(
            {SceneDiagnosticSeverity::error, "light.response_multiplier", std::string {path},
                "light diffuse and specular multipliers must be finite and non-negative"});
    }

    const auto finite_positive = [](double value) {
        return std::isfinite(value) && value > 0.0;
    };
    if ((light.type == SceneLightType::sphere || light.type == SceneLightType::disk
            || light.type == SceneLightType::cylinder)
        && !finite_positive(light.radius)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.radius", std::string {path},
            "sphere, disk, and cylinder light radius must be finite and positive"});
    }
    if (light.type == SceneLightType::rect
        && (!finite_positive(light.width) || !finite_positive(light.height))) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.rect_size",
            std::string {path}, "rect light width and height must be finite and positive"});
    }
    if (light.type == SceneLightType::cylinder && !finite_positive(light.length)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.cylinder_length",
            std::string {path}, "cylinder light length must be finite and positive"});
    }
    if (light.type == SceneLightType::distant && !std::isfinite(light.angle_degrees)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.distant_angle",
            std::string {path}, "distant light angle must be finite"});
    }
    if (light.treat_as_point && light.type != SceneLightType::sphere) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.treat_as_point",
            std::string {path}, "only sphere lights may be treated as point lights"});
    }
    if (light.treat_as_line && light.type != SceneLightType::cylinder) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "light.treat_as_line",
            std::string {path}, "only cylinder lights may be treated as line lights"});
    }
}

SceneMeshGeometry legacy_quad_geometry(const QuadShape& quad) {
    SceneMeshGeometry mesh;
    mesh.points = {quad.origin, quad.origin + quad.edge_u, quad.origin + quad.edge_u + quad.edge_v,
        quad.origin + quad.edge_v};
    mesh.face_vertex_counts = {4};
    mesh.face_vertex_indices = {0, 1, 2, 3};
    return mesh;
}

SceneMeshGeometry legacy_box_geometry(const BoxShape& box) {
    const Eigen::Vector3d& min = box.min_corner;
    const Eigen::Vector3d& max = box.max_corner;
    SceneMeshGeometry mesh;
    mesh.points = {
        {min.x(), min.y(), min.z()},
        {max.x(), min.y(), min.z()},
        {max.x(), max.y(), min.z()},
        {min.x(), max.y(), min.z()},
        {min.x(), min.y(), max.z()},
        {max.x(), min.y(), max.z()},
        {max.x(), max.y(), max.z()},
        {min.x(), max.y(), max.z()},
    };
    mesh.face_vertex_counts = {4, 4, 4, 4, 4, 4};
    mesh.face_vertex_indices = {
        0,
        3,
        2,
        1,
        4,
        5,
        6,
        7,
        0,
        1,
        5,
        4,
        1,
        2,
        6,
        5,
        3,
        7,
        6,
        2,
        0,
        4,
        7,
        3,
    };
    return mesh;
}

template<typename Value>
void append_legacy_primvar(std::vector<ScenePrimvar>& primvars, std::string name,
    ScenePrimvarRole role, const std::vector<Value>& values,
    const std::vector<Eigen::Vector3i>& corner_indices) {
    if (values.empty()) {
        return;
    }
    ScenePrimvar primvar;
    primvar.name = std::move(name);
    primvar.interpolation = corner_indices.empty() ? ScenePrimvarInterpolation::vertex
                                                   : ScenePrimvarInterpolation::face_varying;
    primvar.role = role;
    primvar.values = values;
    if (!corner_indices.empty()) {
        primvar.indices.reserve(corner_indices.size() * 3);
        for (const Eigen::Vector3i& indices : corner_indices) {
            primvar.indices.insert(primvar.indices.end(), {indices.x(), indices.y(), indices.z()});
        }
    }
    primvars.push_back(std::move(primvar));
}

SceneMeshGeometry legacy_triangle_mesh_geometry(const TriangleMeshShape& legacy) {
    SceneMeshGeometry mesh;
    mesh.points = legacy.positions;
    mesh.face_vertex_counts.assign(legacy.triangles.size(), 3);
    mesh.face_vertex_indices.reserve(legacy.triangles.size() * 3);
    for (const Eigen::Vector3i& triangle : legacy.triangles) {
        mesh.face_vertex_indices.insert(mesh.face_vertex_indices.end(),
            {triangle.x(), triangle.y(), triangle.z()});
    }
    append_legacy_primvar(mesh.primvars, "normals", ScenePrimvarRole::normal, legacy.normals,
        legacy.normal_indices);
    append_legacy_primvar(mesh.primvars, "tangents", ScenePrimvarRole::vector, legacy.tangents,
        legacy.tangent_indices);
    append_legacy_primvar(mesh.primvars, "st", ScenePrimvarRole::texcoord, legacy.texcoords,
        legacy.texcoord_indices);
    return mesh;
}

SceneGeometry compile_legacy_geometry(const ShapeDesc& shape) {
    return std::visit(
        [](const auto& geometry) -> SceneGeometry {
            using T = std::decay_t<decltype(geometry)>;
            if constexpr (std::is_same_v<T, SphereShape>) {
                return SceneSphereGeometry {.center = geometry.center, .radius = geometry.radius};
            } else if constexpr (std::is_same_v<T, QuadShape>) {
                return legacy_quad_geometry(geometry);
            } else if constexpr (std::is_same_v<T, BoxShape>) {
                return legacy_box_geometry(geometry);
            } else if constexpr (std::is_same_v<T, TriangleMeshShape>) {
                return legacy_triangle_mesh_geometry(geometry);
            } else {
                static_assert(std::is_same_v<T, void>, "unsupported legacy geometry");
            }
        },
        shape);
}

const ScenePrim& require_prim(const SceneIRv2& scene, std::string_view path) {
    const ScenePrim* prim = scene.find_prim(path);
    if (prim == nullptr) {
        throw std::out_of_range("scene prim not found: " + std::string {path});
    }
    return *prim;
}

} // namespace

SceneStageMetadata& SceneIRv2::stage_metadata() {
    return stage_metadata_;
}

const SceneStageMetadata& SceneIRv2::stage_metadata() const {
    return stage_metadata_;
}

std::size_t SceneIRv2::add_prim(ScenePrim prim) {
    const std::size_t index = prims_.size();
    first_prim_by_path_.try_emplace(prim.path, index);
    prims_.push_back(std::move(prim));
    return index;
}

const std::vector<ScenePrim>& SceneIRv2::prims() const {
    return prims_;
}

const ScenePrim* SceneIRv2::find_prim(std::string_view path) const {
    const auto found = first_prim_by_path_.find(std::string {path});
    if (found == first_prim_by_path_.end()) {
        return nullptr;
    }
    return &prims_[found->second];
}

bool is_valid_scene_prim_path(std::string_view path) {
    if (path.size() < 2 || path.front() != '/' || path.back() == '/') {
        return false;
    }
    std::size_t component_start = 1;
    while (component_start < path.size()) {
        const std::size_t separator = path.find('/', component_start);
        const std::size_t component_end =
            separator == std::string_view::npos ? path.size() : separator;
        if (!is_identifier(path.substr(component_start, component_end - component_start))) {
            return false;
        }
        if (separator == std::string_view::npos) {
            return true;
        }
        component_start = separator + 1;
    }
    return false;
}

std::string parent_scene_prim_path(std::string_view path) {
    if (!is_valid_scene_prim_path(path)) {
        throw std::invalid_argument("invalid scene prim path: " + std::string {path});
    }
    const std::size_t separator = path.find_last_of('/');
    return separator == 0 ? "/" : std::string {path.substr(0, separator)};
}

std::vector<SceneDiagnostic> validate_scene_ir_v2(const SceneIRv2& scene) {
    std::vector<SceneDiagnostic> diagnostics;
    const SceneStageMetadata& stage = scene.stage_metadata();
    if (!std::isfinite(stage.meters_per_unit) || stage.meters_per_unit <= 0.0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "stage.meters_per_unit", {},
            "meters_per_unit must be finite and positive"});
    }
    if (!stage.right_handed) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "stage.handedness", {},
            "OpenUSD geometry stages use right-handed coordinates"});
    }
    if (!std::isfinite(stage.time_codes_per_second) || stage.time_codes_per_second <= 0.0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "stage.time_codes_per_second", {},
            "time_codes_per_second must be finite and positive"});
    }
    if (!std::isfinite(stage.frames_per_second) || stage.frames_per_second <= 0.0) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "stage.frames_per_second", {},
            "frames_per_second must be finite and positive"});
    }
    if (stage.start_time_code && !std::isfinite(*stage.start_time_code)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "stage.start_time_code", {},
            "start_time_code must be finite"});
    }
    if (stage.end_time_code && !std::isfinite(*stage.end_time_code)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "stage.end_time_code", {},
            "end_time_code must be finite"});
    }
    if (stage.start_time_code && stage.end_time_code
        && *stage.start_time_code > *stage.end_time_code) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "stage.time_range", {},
            "start_time_code must not exceed end_time_code"});
    }

    std::unordered_map<std::string, const ScenePrim*> prims_by_path;
    for (const ScenePrim& prim : scene.prims()) {
        if (!is_valid_scene_prim_path(prim.path)) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "prim.path.invalid", prim.path,
                "prim path must be an absolute ASCII prim path"});
            continue;
        }
        if (!prims_by_path.emplace(prim.path, &prim).second) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "prim.path.duplicate", prim.path,
                "prim path must be unique"});
        }
    }

    if (!is_valid_scene_prim_path(stage.default_prim_path)) {
        diagnostics.push_back({SceneDiagnosticSeverity::error, "stage.default_prim_path",
            stage.default_prim_path, "default prim must be an absolute prim path"});
    } else {
        if (parent_scene_prim_path(stage.default_prim_path) != "/") {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "stage.default_prim_not_root",
                stage.default_prim_path, "default prim must be a root prim"});
        }
        if (!prims_by_path.contains(stage.default_prim_path)) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "stage.default_prim_missing",
                stage.default_prim_path, "default prim path does not resolve"});
        }
    }

    for (const ScenePrim& prim : scene.prims()) {
        if (!is_valid_scene_prim_path(prim.path)) {
            continue;
        }
        const std::string parent_path = parent_scene_prim_path(prim.path);
        if (parent_path != "/" && !prims_by_path.contains(parent_path)) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "prim.parent.missing", prim.path,
                "parent prim does not exist: " + parent_path});
        }
        append_transform_diagnostics(diagnostics, prim.local_to_parent, prim.path);

        std::optional<double> previous_time;
        for (const SceneTransformSample& sample : prim.transform_samples) {
            if (!std::isfinite(sample.time_code)) {
                diagnostics.push_back(
                    {SceneDiagnosticSeverity::error, "xform.sample_time.non_finite", prim.path,
                        "transform sample time must be finite"});
            } else if (previous_time && sample.time_code <= *previous_time) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "xform.sample_time.order",
                    prim.path, "transform sample times must be strictly increasing"});
            }
            previous_time = sample.time_code;
            append_transform_diagnostics(diagnostics, sample.local_to_parent, prim.path);
        }

        if (prim.kind == ScenePrimKind::geometry_prototype) {
            if (!prim.geometry) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.payload_missing",
                    prim.path, "geometry prototype requires a geometry payload"});
            } else {
                std::visit(
                    [&](const auto& geometry) {
                        using T = std::decay_t<decltype(geometry)>;
                        if constexpr (std::is_same_v<T, SceneSphereGeometry>) {
                            if (!geometry.center.allFinite() || !std::isfinite(geometry.radius)
                                || geometry.radius <= 0.0) {
                                diagnostics.push_back({SceneDiagnosticSeverity::error,
                                    "geometry.sphere.invalid", prim.path,
                                    "sphere center must be finite and radius must be finite and "
                                    "positive"});
                            }
                        } else if constexpr (std::is_same_v<T, SceneMeshGeometry>) {
                            append_mesh_diagnostics(diagnostics, geometry, prim.path,
                                prims_by_path);
                        }
                    },
                    *prim.geometry);
            }
        } else if (prim.geometry) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "geometry.payload_unexpected",
                prim.path, "only geometry prototype prims may carry geometry payloads"});
        }

        if (prim.kind == ScenePrimKind::camera) {
            if (!prim.camera) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "camera.payload_missing",
                    prim.path, "camera prim requires a camera payload"});
            } else {
                append_camera_diagnostics(diagnostics, *prim.camera, prim.path);
            }
        } else if (prim.camera) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "camera.payload_unexpected",
                prim.path, "only camera prims may carry camera payloads"});
        }

        if (prim.kind == ScenePrimKind::light) {
            if (!prim.light) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "light.payload_missing",
                    prim.path, "light prim requires a light payload"});
            } else if (prim.light->type == SceneLightType::geometry) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "light.payload_kind",
                    prim.path, "geometry light payloads must be applied to surface prims"});
            }
        } else if (prim.kind == ScenePrimKind::surface && prim.light) {
            if (prim.light->type != SceneLightType::geometry) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "light.payload_kind",
                    prim.path, "surface prims may only carry geometry light payloads"});
            }
        } else if (prim.light) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "light.payload_unexpected",
                prim.path, "only light and surface prims may carry light payloads"});
        }
        if (prim.light) {
            append_light_diagnostics(diagnostics, *prim.light, prim.path);
        }

        for (const SceneAssetReference& asset : prim.asset_references) {
            append_asset_diagnostics(diagnostics, asset, prim.path);
        }

        if (prim.kind == ScenePrimKind::surface || prim.kind == ScenePrimKind::volume) {
            const auto prototype = prims_by_path.find(prim.prototype_path);
            if (prototype == prims_by_path.end()
                || prototype->second->kind != ScenePrimKind::geometry_prototype) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "reference.prototype",
                    prim.path,
                    "surface or volume prototype path does not resolve to a geometry prototype"});
            }
            const auto material = prims_by_path.find(prim.material_path);
            if (material == prims_by_path.end()
                || material->second->kind != ScenePrimKind::material) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "reference.material",
                    prim.path, "surface or volume material path does not resolve to a material"});
            }
        }
        if (prim.kind == ScenePrimKind::volume
            && (!prim.volume_density || !std::isfinite(*prim.volume_density)
                || *prim.volume_density <= 0.0)) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "volume.density", prim.path,
                "volume density must be finite and positive"});
        }
    }
    return diagnostics;
}

void require_valid_scene_ir_v2(const SceneIRv2& scene) {
    const std::vector<SceneDiagnostic> diagnostics = validate_scene_ir_v2(scene);
    const auto error =
        std::find_if(diagnostics.begin(), diagnostics.end(), [](const SceneDiagnostic& diagnostic) {
            return diagnostic.severity == SceneDiagnosticSeverity::error;
        });
    if (error != diagnostics.end()) {
        throw std::invalid_argument(error->code + ": " + error->message);
    }
}

bool has_scene_diagnostic(const std::vector<SceneDiagnostic>& diagnostics, std::string_view code) {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
        [&](const SceneDiagnostic& diagnostic) { return diagnostic.code == code; });
}

std::vector<SceneDiagnostic> diagnose_scene_ir_v2_capabilities(const SceneIRv2& scene,
    const SceneBackendCapabilities& capabilities) {
    std::vector<SceneDiagnostic> diagnostics;
    for (const ScenePrim& prim : scene.prims()) {
        if (!capabilities.full_affine_transforms) {
            const bool has_non_rigid_transform =
                !is_rigid_transform(prim.local_to_parent)
                || std::any_of(prim.transform_samples.begin(), prim.transform_samples.end(),
                    [](const SceneTransformSample& sample) {
                        return !is_rigid_transform(sample.local_to_parent);
                    });
            if (has_non_rigid_transform) {
                diagnostics.push_back(
                    {SceneDiagnosticSeverity::error, "capability.full_affine_transform", prim.path,
                        capabilities.backend_name + " does not support scale or shear transforms"});
            }
        }
        if (!capabilities.transform_time_samples && !prim.transform_samples.empty()) {
            diagnostics.push_back(
                {SceneDiagnosticSeverity::error, "capability.transform_time_samples", prim.path,
                    capabilities.backend_name + " does not support transform time samples"});
        }
        if (!capabilities.reset_xform_stack && prim.reset_xform_stack) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "capability.reset_xform_stack",
                prim.path, capabilities.backend_name + " does not support reset_xform_stack"});
        }
        if (!capabilities.non_render_purposes && prim.authored_purpose
            && (*prim.authored_purpose == ScenePurpose::proxy
                || *prim.authored_purpose == ScenePurpose::guide)) {
            diagnostics.push_back({SceneDiagnosticSeverity::error, "capability.non_render_purpose",
                prim.path, capabilities.backend_name + " does not support proxy or guide purpose"});
        }
        if (prim.camera) {
            const SceneCamera& camera = *prim.camera;
            if (!capabilities.orthographic_cameras
                && camera.projection == SceneCameraProjection::orthographic) {
                diagnostics.push_back(
                    {SceneDiagnosticSeverity::error, "capability.orthographic_camera", prim.path,
                        capabilities.backend_name + " does not support orthographic cameras"});
            }
            if (camera.renderer_calibration) {
                const SceneCameraCalibration& calibration = *camera.renderer_calibration;
                if (!capabilities.camera_model_extensions
                    && calibration.model == SceneCameraCalibrationModel::equi62_lut1d) {
                    diagnostics.push_back({SceneDiagnosticSeverity::error,
                        "capability.camera_model_extension", prim.path,
                        capabilities.backend_name + " does not support equi62_lut1d cameras"});
                }
                if (!capabilities.camera_distortion && camera_has_distortion(calibration)) {
                    diagnostics.push_back(
                        {SceneDiagnosticSeverity::error, "capability.camera_distortion", prim.path,
                            capabilities.backend_name + " does not support camera distortion"});
                }
            }
        }
        if (!capabilities.asset_references && !prim.asset_references.empty()) {
            diagnostics.push_back(
                {SceneDiagnosticSeverity::error, "capability.asset_references", prim.path,
                    capabilities.backend_name
                        + " does not preserve authored and resolved asset paths"});
        }
        if (prim.light) {
            const SceneLight& light = *prim.light;
            if (!capabilities.usd_lux_lights) {
                diagnostics.push_back(
                    {SceneDiagnosticSeverity::error, "capability.usd_lux_lights", prim.path,
                        capabilities.backend_name + " does not support UsdLux light payloads"});
            }
            if (!capabilities.analytic_lights && light.type != SceneLightType::geometry
                && light.type != SceneLightType::dome) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "capability.analytic_lights",
                    prim.path, capabilities.backend_name + " does not support analytic lights"});
            }
            if (!capabilities.dome_lights && light.type == SceneLightType::dome) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "capability.dome_lights",
                    prim.path, capabilities.backend_name + " does not support dome lights"});
            }
            if (!capabilities.geometry_lights && light.type == SceneLightType::geometry) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "capability.geometry_lights",
                    prim.path, capabilities.backend_name + " does not support geometry lights"});
            }
            if (!capabilities.light_normalization && light.normalize) {
                diagnostics.push_back(
                    {SceneDiagnosticSeverity::error, "capability.light_normalization", prim.path,
                        capabilities.backend_name + " does not support UsdLux size normalization"});
            }
            if (!capabilities.light_color_temperature && light.enable_color_temperature) {
                diagnostics.push_back({SceneDiagnosticSeverity::error,
                    "capability.light_color_temperature", prim.path,
                    capabilities.backend_name + " does not support UsdLux color temperature"});
            }
        }
        if (prim.geometry) {
            const auto* mesh = std::get_if<SceneMeshGeometry>(&*prim.geometry);
            if (mesh == nullptr) {
                continue;
            }
            if (!capabilities.mesh_ngons
                && std::any_of(mesh->face_vertex_counts.begin(), mesh->face_vertex_counts.end(),
                    [](std::int32_t count) { return count != 3; })) {
                diagnostics.push_back(
                    {SceneDiagnosticSeverity::error, "capability.mesh_ngons", prim.path,
                        capabilities.backend_name + " does not support non-triangle mesh faces"});
            }
            if (!capabilities.mesh_primvars && !mesh->primvars.empty()) {
                diagnostics.push_back({SceneDiagnosticSeverity::error, "capability.mesh_primvars",
                    prim.path, capabilities.backend_name + " does not support mesh primvars"});
            }
            if (!capabilities.subdivision_surfaces
                && mesh->subdivision_scheme != SceneSubdivisionScheme::none) {
                diagnostics.push_back(
                    {SceneDiagnosticSeverity::error, "capability.subdivision_surfaces", prim.path,
                        capabilities.backend_name + " does not support subdivision surfaces"});
            }
            if (!capabilities.material_subsets && !mesh->material_subsets.empty()) {
                diagnostics.push_back(
                    {SceneDiagnosticSeverity::error, "capability.material_subsets", prim.path,
                        capabilities.backend_name + " does not support material subsets"});
            }
        }
    }
    return diagnostics;
}

Eigen::Matrix4d sample_scene_local_transform(const ScenePrim& prim, double time_code,
    SceneTimeInterpolation interpolation) {
    if (prim.transform_samples.empty()) {
        return prim.local_to_parent;
    }
    const auto upper = std::lower_bound(prim.transform_samples.begin(),
        prim.transform_samples.end(), time_code,
        [](const SceneTransformSample& sample, double time) { return sample.time_code < time; });
    if (upper == prim.transform_samples.begin()) {
        return upper->local_to_parent;
    }
    if (upper == prim.transform_samples.end()) {
        return prim.transform_samples.back().local_to_parent;
    }
    if (upper->time_code == time_code) {
        return upper->local_to_parent;
    }
    if (interpolation == SceneTimeInterpolation::held) {
        return std::prev(upper)->local_to_parent;
    }
    const SceneTransformSample& lower = *std::prev(upper);
    const double alpha = (time_code - lower.time_code) / (upper->time_code - lower.time_code);
    Eigen::Matrix4d sampled =
        (1.0 - alpha) * lower.local_to_parent + alpha * upper->local_to_parent;
    sampled.row(3) = Eigen::RowVector4d {0.0, 0.0, 0.0, 1.0};
    return sampled;
}

Eigen::Matrix4d compute_scene_world_transform(const SceneIRv2& scene, std::string_view prim_path,
    double time_code) {
    std::vector<const ScenePrim*> lineage;
    std::string current_path {prim_path};
    while (current_path != "/") {
        const ScenePrim& prim = require_prim(scene, current_path);
        lineage.push_back(&prim);
        current_path = parent_scene_prim_path(current_path);
    }

    Eigen::Matrix4d world = Eigen::Matrix4d::Identity();
    for (auto it = lineage.rbegin(); it != lineage.rend(); ++it) {
        const ScenePrim& prim = **it;
        const Eigen::Matrix4d local =
            sample_scene_local_transform(prim, time_code, scene.stage_metadata().interpolation);
        world = prim.reset_xform_stack ? local : world * local;
    }
    return world;
}

bool compute_scene_visibility(const SceneIRv2& scene, std::string_view prim_path) {
    std::string current_path {prim_path};
    while (current_path != "/") {
        const ScenePrim& prim = require_prim(scene, current_path);
        if (prim.visibility == SceneVisibility::invisible) {
            return false;
        }
        current_path = parent_scene_prim_path(current_path);
    }
    return true;
}

ScenePurpose compute_scene_purpose(const SceneIRv2& scene, std::string_view prim_path) {
    std::string current_path {prim_path};
    while (current_path != "/") {
        const ScenePrim& prim = require_prim(scene, current_path);
        if (prim.authored_purpose) {
            return *prim.authored_purpose;
        }
        current_path = parent_scene_prim_path(current_path);
    }
    return ScenePurpose::default_;
}

double scene_light_exposed_intensity(const SceneLight& light) {
    return light.intensity * std::exp2(light.exposure);
}

SceneLightIntensityUnit scene_light_intensity_unit(const SceneLight& light) {
    return light.type == SceneLightType::distant && light.normalize ? SceneLightIntensityUnit::lux
                                                                    : SceneLightIntensityUnit::nit;
}

SceneIRv2 compile_legacy_scene_ir_v2(const SceneIR& legacy_scene) {
    SceneStageMetadata metadata;
    metadata.meters_per_unit = 1.0;
    return compile_legacy_scene_ir_v2(legacy_scene, std::move(metadata));
}

SceneIRv2 compile_legacy_scene_ir_v2(const SceneIR& legacy_scene, SceneStageMetadata metadata) {
    validate_scene_ir(legacy_scene);

    SceneIRv2 scene;
    scene.stage_metadata() = std::move(metadata);
    scene.add_prim(ScenePrim {.path = "/World"});
    scene.add_prim(ScenePrim {.path = "/World/Textures"});
    scene.add_prim(ScenePrim {.path = "/World/Materials"});
    scene.add_prim(ScenePrim {.path = "/World/Prototypes"});
    scene.add_prim(ScenePrim {.path = "/World/Geometry"});
    scene.add_prim(ScenePrim {.path = "/World/Volumes"});

    for (std::size_t index = 0; index < legacy_scene.textures().size(); ++index) {
        ScenePrim texture_prim {
            .path = indexed_path("/World/Textures", "Texture", index),
            .kind = ScenePrimKind::texture,
            .compatibility_source_index = index,
        };
        if (const auto* image = std::get_if<ImageTextureDesc>(&legacy_scene.textures()[index])) {
            texture_prim.asset_references.push_back(SceneAssetReference {
                .authored_path = image->authored_path.empty() ? image->path : image->authored_path,
                .resolved_path = image->path,
            });
        }
        scene.add_prim(std::move(texture_prim));
    }
    for (std::size_t index = 0; index < legacy_scene.materials().size(); ++index) {
        scene.add_prim(ScenePrim {
            .path = indexed_path("/World/Materials", "Material", index),
            .kind = ScenePrimKind::material,
            .compatibility_source_index = index,
        });
    }
    for (std::size_t index = 0; index < legacy_scene.shapes().size(); ++index) {
        scene.add_prim(ScenePrim {
            .path = indexed_path("/World/Prototypes", "Shape", index),
            .kind = ScenePrimKind::geometry_prototype,
            .geometry = compile_legacy_geometry(legacy_scene.shapes()[index]),
            .compatibility_source_index = index,
        });
    }
    for (std::size_t index = 0; index < legacy_scene.surface_instances().size(); ++index) {
        const SurfaceInstance& instance = legacy_scene.surface_instances()[index];
        ScenePrim surface {
            .path = indexed_path("/World/Geometry", "Surface", index),
            .kind = ScenePrimKind::surface,
            .local_to_parent = legacy_transform_matrix(instance.transform),
            .prototype_path = indexed_path("/World/Prototypes", "Shape",
                static_cast<std::size_t>(instance.shape_index)),
            .material_path = indexed_path("/World/Materials", "Material",
                static_cast<std::size_t>(instance.material_index)),
            .compatibility_source_index = index,
        };
        if (std::holds_alternative<EmissiveMaterial>(
                legacy_scene.materials()[static_cast<std::size_t>(instance.material_index)])) {
            surface.light = SceneLight {
                .type = SceneLightType::geometry,
                .material_sync_mode = SceneLightMaterialSyncMode::material_glow_tints_light,
            };
        }
        scene.add_prim(std::move(surface));
    }
    for (std::size_t index = 0; index < legacy_scene.media().size(); ++index) {
        const MediumInstance& medium = legacy_scene.media()[index];
        scene.add_prim(ScenePrim {
            .path = indexed_path("/World/Volumes", "Medium", index),
            .kind = ScenePrimKind::volume,
            .local_to_parent = legacy_transform_matrix(medium.transform),
            .prototype_path = indexed_path("/World/Prototypes", "Shape",
                static_cast<std::size_t>(medium.shape_index)),
            .material_path = indexed_path("/World/Materials", "Material",
                static_cast<std::size_t>(medium.material_index)),
            .volume_density = medium.density,
            .compatibility_source_index = index,
        });
    }
    require_valid_scene_ir_v2(scene);
    return scene;
}

} // namespace rt::scene
