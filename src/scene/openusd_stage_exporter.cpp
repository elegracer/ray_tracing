#include "scene/openusd_stage_exporter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#if RT_HAS_OPENUSD
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/types.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/schema.h>
#include <pxr/usd/usd/primDefinition.h>
#include <pxr/usd/usd/interpolation.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/references.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformOp.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/usdLux/tokens.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/output.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/tokens.h>
#endif

namespace rt::scene {

bool openusd_stage_exporter_available() {
#if RT_HAS_OPENUSD
    return true;
#else
    return false;
#endif
}

#if RT_HAS_OPENUSD
namespace {

constexpr std::string_view kCompilerInternalRoot = "/__SceneIR";

void require_authored(bool success, const std::string& message) {
    if (!success) {
        throw std::runtime_error(message);
    }
}

bool is_compiler_internal_path(std::string_view path) {
    return path == kCompilerInternalRoot
           || (path.size() > kCompilerInternalRoot.size()
               && path.substr(0, kCompilerInternalRoot.size()) == kCompilerInternalRoot
               && path[kCompilerInternalRoot.size()] == '/');
}

std::string prototype_class_path(std::string_view prototype_path) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : prototype_path) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream path;
    path << "/__SceneIRPrototype_" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return path.str();
}

pxr::GfMatrix4d to_gf_transform(const Eigen::Matrix4d& value) {
    pxr::GfMatrix4d result;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            result[row][column] = value(column, row);
        }
    }
    return result;
}

pxr::GfVec3f to_gf_vector(const Eigen::Vector3d& value) {
    return {static_cast<float>(value.x()), static_cast<float>(value.y()),
        static_cast<float>(value.z())};
}

pxr::TfToken scene_purpose_token(ScenePurpose purpose) {
    switch (purpose) {
        case ScenePurpose::default_: return pxr::UsdGeomTokens->default_;
        case ScenePurpose::render: return pxr::UsdGeomTokens->render;
        case ScenePurpose::proxy: return pxr::UsdGeomTokens->proxy;
        case ScenePurpose::guide: return pxr::UsdGeomTokens->guide;
    }
    throw std::logic_error("unreachable ScenePurpose value");
}

void author_transform(const ScenePrim& source, const pxr::UsdPrim& destination) {
    const pxr::UsdGeomXformable xformable {destination};
    if (!xformable) {
        throw std::invalid_argument(
            "SceneIR prim is not exportable as an OpenUSD xformable: " + source.path);
    }

    const bool has_transform = source.reset_xform_stack || !source.transform_samples.empty()
                               || !source.local_to_parent.isApprox(Eigen::Matrix4d::Identity());
    if (!has_transform) {
        return;
    }

    const pxr::UsdGeomXformOp transform = xformable.MakeMatrixXform();
    require_authored(static_cast<bool>(transform),
        "failed to create OpenUSD matrix xform on " + source.path);
    require_authored(transform.Set(to_gf_transform(source.local_to_parent)),
        "failed to author OpenUSD default transform on " + source.path);
    for (const SceneTransformSample& sample : source.transform_samples) {
        require_authored(transform.Set(to_gf_transform(sample.local_to_parent),
                             pxr::UsdTimeCode {sample.time_code}),
            "failed to author OpenUSD transform sample on " + source.path);
    }
    require_authored(xformable.SetResetXformStack(source.reset_xform_stack),
        "failed to author OpenUSD reset-xform-stack on " + source.path);
}

void author_imageable(const ScenePrim& source, const pxr::UsdPrim& destination) {
    const pxr::UsdGeomImageable imageable {destination};
    if (!imageable) {
        throw std::invalid_argument(
            "SceneIR prim is not exportable as an OpenUSD imageable: " + source.path);
    }
    if (source.visibility == SceneVisibility::invisible) {
        require_authored(imageable.CreateVisibilityAttr().Set(pxr::UsdGeomTokens->invisible),
            "failed to author OpenUSD visibility on " + source.path);
    }
    if (source.authored_purpose) {
        require_authored(
            imageable.CreatePurposeAttr().Set(scene_purpose_token(*source.authored_purpose)),
            "failed to author OpenUSD purpose on " + source.path);
    }
}

void author_common_prim(const ScenePrim& source, const pxr::UsdPrim& destination) {
    author_transform(source, destination);
    author_imageable(source, destination);
}

pxr::TfToken light_material_sync_token(SceneLightMaterialSyncMode mode) {
    switch (mode) {
        case SceneLightMaterialSyncMode::material_glow_tints_light:
            return pxr::UsdLuxTokens->materialGlowTintsLight;
        case SceneLightMaterialSyncMode::independent: return pxr::UsdLuxTokens->independent;
        case SceneLightMaterialSyncMode::no_material_response:
            return pxr::UsdLuxTokens->noMaterialResponse;
    }
    throw std::logic_error("unreachable SceneLightMaterialSyncMode value");
}

void author_light_asset(const ScenePrim& source, const pxr::UsdAttribute& attribute) {
    if (source.asset_references.size() > 1) {
        throw std::invalid_argument(
            "USD-04 supports at most one texture asset per light: " + source.path);
    }
    if (source.asset_references.empty()) {
        return;
    }
    const SceneAssetReference& asset = source.asset_references.front();
    require_authored(attribute.Set(pxr::SdfAssetPath {asset.authored_path}),
        "failed to author OpenUSD light texture asset on " + source.path);
}

pxr::UsdPrim author_light(const ScenePrim& source, const pxr::UsdStageRefPtr& stage) {
    const SceneLight& light = *source.light;
    const pxr::SdfPath path {source.path};
    pxr::UsdPrim prim;

    switch (light.type) {
        case SceneLightType::sphere: {
            const pxr::UsdLuxSphereLight schema = pxr::UsdLuxSphereLight::Define(stage, path);
            prim = schema.GetPrim();
            require_authored(schema.CreateRadiusAttr().Set(static_cast<float>(light.radius)),
                "failed to author sphere-light radius on " + source.path);
            require_authored(schema.CreateTreatAsPointAttr().Set(light.treat_as_point),
                "failed to author sphere-light point mode on " + source.path);
            break;
        }
        case SceneLightType::disk: {
            const pxr::UsdLuxDiskLight schema = pxr::UsdLuxDiskLight::Define(stage, path);
            prim = schema.GetPrim();
            require_authored(schema.CreateRadiusAttr().Set(static_cast<float>(light.radius)),
                "failed to author disk-light radius on " + source.path);
            break;
        }
        case SceneLightType::rect: {
            const pxr::UsdLuxRectLight schema = pxr::UsdLuxRectLight::Define(stage, path);
            prim = schema.GetPrim();
            require_authored(schema.CreateWidthAttr().Set(static_cast<float>(light.width)),
                "failed to author rect-light width on " + source.path);
            require_authored(schema.CreateHeightAttr().Set(static_cast<float>(light.height)),
                "failed to author rect-light height on " + source.path);
            author_light_asset(source, schema.CreateTextureFileAttr());
            break;
        }
        case SceneLightType::cylinder: {
            const pxr::UsdLuxCylinderLight schema = pxr::UsdLuxCylinderLight::Define(stage, path);
            prim = schema.GetPrim();
            require_authored(schema.CreateRadiusAttr().Set(static_cast<float>(light.radius)),
                "failed to author cylinder-light radius on " + source.path);
            require_authored(schema.CreateLengthAttr().Set(static_cast<float>(light.length)),
                "failed to author cylinder-light length on " + source.path);
            require_authored(schema.CreateTreatAsLineAttr().Set(light.treat_as_line),
                "failed to author cylinder-light line mode on " + source.path);
            break;
        }
        case SceneLightType::distant: {
            const pxr::UsdLuxDistantLight schema = pxr::UsdLuxDistantLight::Define(stage, path);
            prim = schema.GetPrim();
            require_authored(schema.CreateAngleAttr().Set(static_cast<float>(light.angle_degrees)),
                "failed to author distant-light angle on " + source.path);
            break;
        }
        case SceneLightType::dome: {
            const pxr::UsdLuxDomeLight schema = pxr::UsdLuxDomeLight::Define(stage, path);
            prim = schema.GetPrim();
            author_light_asset(source, schema.CreateTextureFileAttr());
            break;
        }
        case SceneLightType::geometry:
            throw std::invalid_argument(
                "USD-04 does not export renderer geometry-light payloads: " + source.path);
    }

    if (light.type != SceneLightType::rect && light.type != SceneLightType::dome
        && !source.asset_references.empty()) {
        throw std::invalid_argument(
            "only rect and dome lights may carry texture assets in USD-04: " + source.path);
    }

    const pxr::UsdLuxLightAPI api {prim};
    require_authored(api.CreateColorAttr().Set(to_gf_vector(light.color)),
        "failed to author light color on " + source.path);
    require_authored(api.CreateIntensityAttr().Set(static_cast<float>(light.intensity)),
        "failed to author light intensity on " + source.path);
    require_authored(api.CreateExposureAttr().Set(static_cast<float>(light.exposure)),
        "failed to author light exposure on " + source.path);
    require_authored(api.CreateNormalizeAttr().Set(light.normalize),
        "failed to author light normalization on " + source.path);
    require_authored(api.CreateEnableColorTemperatureAttr().Set(light.enable_color_temperature),
        "failed to author light color-temperature mode on " + source.path);
    require_authored(
        api.CreateColorTemperatureAttr().Set(static_cast<float>(light.color_temperature_kelvin)),
        "failed to author light color temperature on " + source.path);
    require_authored(api.CreateDiffuseAttr().Set(static_cast<float>(light.diffuse)),
        "failed to author light diffuse weight on " + source.path);
    require_authored(api.CreateSpecularAttr().Set(static_cast<float>(light.specular)),
        "failed to author light specular weight on " + source.path);
    require_authored(
        api.CreateMaterialSyncModeAttr().Set(light_material_sync_token(light.material_sync_mode)),
        "failed to author light material-sync mode on " + source.path);
    return prim;
}

pxr::UsdPrim author_camera(const ScenePrim& source, const pxr::UsdStageRefPtr& stage) {
    const SceneCamera& camera = *source.camera;
    if (camera.renderer_calibration) {
        throw std::invalid_argument(
            "USD-04 does not serialize renderer camera calibration extensions: " + source.path);
    }
    const pxr::UsdGeomCamera schema = pxr::UsdGeomCamera::Define(stage, pxr::SdfPath {source.path});
    require_authored(
        schema.CreateProjectionAttr().Set(camera.projection == SceneCameraProjection::orthographic
                                              ? pxr::UsdGeomTokens->orthographic
                                              : pxr::UsdGeomTokens->perspective),
        "failed to author camera projection on " + source.path);
    require_authored(
        schema.CreateHorizontalApertureAttr().Set(static_cast<float>(camera.horizontal_aperture)),
        "failed to author camera horizontal aperture on " + source.path);
    require_authored(
        schema.CreateVerticalApertureAttr().Set(static_cast<float>(camera.vertical_aperture)),
        "failed to author camera vertical aperture on " + source.path);
    require_authored(schema.CreateHorizontalApertureOffsetAttr().Set(
                         static_cast<float>(camera.horizontal_aperture_offset)),
        "failed to author camera horizontal offset on " + source.path);
    require_authored(schema.CreateVerticalApertureOffsetAttr().Set(
                         static_cast<float>(camera.vertical_aperture_offset)),
        "failed to author camera vertical offset on " + source.path);
    require_authored(schema.CreateFocalLengthAttr().Set(static_cast<float>(camera.focal_length)),
        "failed to author camera focal length on " + source.path);
    require_authored(schema.CreateClippingRangeAttr().Set(
                         pxr::GfVec2f {static_cast<float>(camera.clipping_range.x()),
                             static_cast<float>(camera.clipping_range.y())}),
        "failed to author camera clipping range on " + source.path);
    require_authored(schema.CreateFStopAttr().Set(static_cast<float>(camera.f_stop)),
        "failed to author camera f-stop on " + source.path);
    require_authored(
        schema.CreateFocusDistanceAttr().Set(static_cast<float>(camera.focus_distance)),
        "failed to author camera focus distance on " + source.path);
    return schema.GetPrim();
}

void author_openpbr_material(const ScenePrim& source, const pxr::UsdStageRefPtr& stage) {
    if (!std::holds_alternative<SceneOpenPbrSurface>(*source.material)) {
        throw std::invalid_argument(
            "USD-04 supports OpenPBR surface materials only: " + source.path);
    }
    const SceneOpenPbrSurface& surface = std::get<SceneOpenPbrSurface>(*source.material);
    if (!surface.connections.empty()
        || surface.displacement.type != SceneMaterialXDisplacementType::none) {
        throw std::invalid_argument(
            "USD-04 connected inputs and displacement remain fail-closed: " + source.path);
    }

    const pxr::SdfPath material_path {source.path};
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, material_path);
    pxr::UsdShadeShader shader =
        pxr::UsdShadeShader::Define(stage, material_path.AppendChild(pxr::TfToken {"OpenPBR"}));
    require_authored(
        shader.CreateIdAttr().Set(pxr::TfToken {std::string {kOpenPbrMaterialXNodeDef}}),
        "failed to author OpenPBR shader id on " + source.path);

    using ScalarMember = double SceneOpenPbrSurface::*;
    static const std::array<std::pair<std::string_view, ScalarMember>, 27> scalar_inputs {{
        {"base_weight", &SceneOpenPbrSurface::base_weight},
        {"base_diffuse_roughness", &SceneOpenPbrSurface::base_diffuse_roughness},
        {"base_metalness", &SceneOpenPbrSurface::base_metalness},
        {"specular_weight", &SceneOpenPbrSurface::specular_weight},
        {"specular_roughness", &SceneOpenPbrSurface::specular_roughness},
        {"specular_ior", &SceneOpenPbrSurface::specular_ior},
        {"specular_roughness_anisotropy", &SceneOpenPbrSurface::specular_roughness_anisotropy},
        {"transmission_weight", &SceneOpenPbrSurface::transmission_weight},
        {"transmission_depth", &SceneOpenPbrSurface::transmission_depth},
        {"transmission_scatter_anisotropy", &SceneOpenPbrSurface::transmission_scatter_anisotropy},
        {"transmission_dispersion_scale", &SceneOpenPbrSurface::transmission_dispersion_scale},
        {"transmission_dispersion_abbe_number",
            &SceneOpenPbrSurface::transmission_dispersion_abbe_number},
        {"subsurface_weight", &SceneOpenPbrSurface::subsurface_weight},
        {"subsurface_radius", &SceneOpenPbrSurface::subsurface_radius},
        {"subsurface_scatter_anisotropy", &SceneOpenPbrSurface::subsurface_scatter_anisotropy},
        {"fuzz_weight", &SceneOpenPbrSurface::fuzz_weight},
        {"fuzz_roughness", &SceneOpenPbrSurface::fuzz_roughness},
        {"coat_weight", &SceneOpenPbrSurface::coat_weight},
        {"coat_roughness", &SceneOpenPbrSurface::coat_roughness},
        {"coat_roughness_anisotropy", &SceneOpenPbrSurface::coat_roughness_anisotropy},
        {"coat_ior", &SceneOpenPbrSurface::coat_ior},
        {"coat_darkening", &SceneOpenPbrSurface::coat_darkening},
        {"thin_film_weight", &SceneOpenPbrSurface::thin_film_weight},
        {"thin_film_thickness", &SceneOpenPbrSurface::thin_film_thickness},
        {"thin_film_ior", &SceneOpenPbrSurface::thin_film_ior},
        {"emission_luminance", &SceneOpenPbrSurface::emission_luminance},
        {"geometry_opacity", &SceneOpenPbrSurface::geometry_opacity},
    }};
    for (const auto& input : scalar_inputs) {
        require_authored(shader
                             .CreateInput(pxr::TfToken {std::string {input.first}},
                                 pxr::SdfValueTypeNames->Float)
                             .Set(static_cast<float>(surface.*input.second)),
            "failed to author OpenPBR scalar input on " + source.path);
    }

    using ColorMember = Eigen::Vector3d SceneOpenPbrSurface::*;
    static const std::array<std::pair<std::string_view, ColorMember>, 9> color_inputs {{
        {"base_color", &SceneOpenPbrSurface::base_color},
        {"specular_color", &SceneOpenPbrSurface::specular_color},
        {"transmission_color", &SceneOpenPbrSurface::transmission_color},
        {"transmission_scatter", &SceneOpenPbrSurface::transmission_scatter},
        {"subsurface_color", &SceneOpenPbrSurface::subsurface_color},
        {"subsurface_radius_scale", &SceneOpenPbrSurface::subsurface_radius_scale},
        {"fuzz_color", &SceneOpenPbrSurface::fuzz_color},
        {"coat_color", &SceneOpenPbrSurface::coat_color},
        {"emission_color", &SceneOpenPbrSurface::emission_color},
    }};
    for (const auto& input : color_inputs) {
        require_authored(shader
                             .CreateInput(pxr::TfToken {std::string {input.first}},
                                 pxr::SdfValueTypeNames->Color3f)
                             .Set(to_gf_vector(surface.*input.second)),
            "failed to author OpenPBR color input on " + source.path);
    }
    require_authored(
        shader.CreateInput(pxr::TfToken {"geometry_thin_walled"}, pxr::SdfValueTypeNames->Bool)
            .Set(surface.geometry_thin_walled),
        "failed to author OpenPBR thin-walled input on " + source.path);

    const pxr::UsdShadeOutput shader_output =
        shader.CreateOutput(pxr::TfToken {"out"}, pxr::SdfValueTypeNames->Token);
    require_authored(material.CreateSurfaceOutput().ConnectToSource(shader_output),
        "failed to connect OpenPBR material surface on " + source.path);
}

pxr::TfToken mesh_orientation_token(SceneMeshOrientation orientation) {
    return orientation == SceneMeshOrientation::left_handed ? pxr::UsdGeomTokens->leftHanded
                                                            : pxr::UsdGeomTokens->rightHanded;
}

pxr::TfToken subdivision_token(SceneSubdivisionScheme scheme) {
    switch (scheme) {
        case SceneSubdivisionScheme::none: return pxr::UsdGeomTokens->none;
        case SceneSubdivisionScheme::catmull_clark: return pxr::UsdGeomTokens->catmullClark;
        case SceneSubdivisionScheme::loop: return pxr::UsdGeomTokens->loop;
        case SceneSubdivisionScheme::bilinear: return pxr::UsdGeomTokens->bilinear;
    }
    throw std::logic_error("unreachable SceneSubdivisionScheme value");
}

void author_geometry_class(const ScenePrim& source, const std::string& class_path,
    const pxr::UsdStageRefPtr& stage) {
    const pxr::UsdPrim class_prim = stage->CreateClassPrim(pxr::SdfPath {class_path});
    require_authored(static_cast<bool>(class_prim),
        "failed to create OpenUSD geometry class for " + source.path);

    std::visit(
        [&](const auto& geometry) {
            using T = std::decay_t<decltype(geometry)>;
            if constexpr (std::is_same_v<T, SceneSphereGeometry>) {
                require_authored(class_prim.SetTypeName(pxr::TfToken {"Sphere"}),
                    "failed to type OpenUSD sphere class for " + source.path);
                const pxr::UsdGeomSphere sphere {class_prim};
                require_authored(sphere.CreateRadiusAttr().Set(geometry.radius),
                    "failed to author OpenUSD sphere class for " + source.path);
            } else if constexpr (std::is_same_v<T, SceneMeshGeometry>) {
                if (!geometry.primvars.empty() || !geometry.material_subsets.empty()) {
                    throw std::invalid_argument(
                        "USD-04 mesh primvars and material subsets remain fail-closed: "
                        + source.path);
                }
                require_authored(class_prim.SetTypeName(pxr::TfToken {"Mesh"}),
                    "failed to type OpenUSD mesh class for " + source.path);
                const pxr::UsdGeomMesh mesh {class_prim};
                pxr::VtVec3fArray points;
                points.reserve(geometry.points.size());
                for (const Eigen::Vector3d& point : geometry.points) {
                    points.push_back(to_gf_vector(point));
                }
                pxr::VtIntArray counts(geometry.face_vertex_counts.begin(),
                    geometry.face_vertex_counts.end());
                pxr::VtIntArray indices(geometry.face_vertex_indices.begin(),
                    geometry.face_vertex_indices.end());
                require_authored(mesh.CreatePointsAttr().Set(points),
                    "failed to author OpenUSD mesh points for " + source.path);
                require_authored(mesh.CreateFaceVertexCountsAttr().Set(counts),
                    "failed to author OpenUSD mesh counts for " + source.path);
                require_authored(mesh.CreateFaceVertexIndicesAttr().Set(indices),
                    "failed to author OpenUSD mesh indices for " + source.path);
                require_authored(
                    mesh.CreateOrientationAttr().Set(mesh_orientation_token(geometry.orientation)),
                    "failed to author OpenUSD mesh orientation for " + source.path);
                require_authored(mesh.CreateSubdivisionSchemeAttr().Set(
                                     subdivision_token(geometry.subdivision_scheme)),
                    "failed to author OpenUSD mesh subdivision for " + source.path);
            }
        },
        *source.geometry);
}

std::vector<const ScenePrim*> ordered_prims(const SceneIRv2& scene) {
    std::vector<const ScenePrim*> result;
    result.reserve(scene.prims().size());
    for (const ScenePrim& prim : scene.prims()) {
        result.push_back(&prim);
    }
    std::sort(result.begin(), result.end(), [](const ScenePrim* left, const ScenePrim* right) {
        const auto left_depth = std::count(left->path.begin(), left->path.end(), '/');
        const auto right_depth = std::count(right->path.begin(), right->path.end(), '/');
        return left_depth != right_depth ? left_depth < right_depth : left->path < right->path;
    });
    return result;
}

void author_surface(const ScenePrim& source, const pxr::UsdStageRefPtr& stage,
    const std::unordered_map<std::string, std::string>& class_path_by_prototype) {
    if (source.light) {
        throw std::invalid_argument(
            "USD-04 geometry-light export remains fail-closed: " + source.path);
    }
    const auto prototype = class_path_by_prototype.find(source.prototype_path);
    if (prototype == class_path_by_prototype.end()) {
        throw std::logic_error(
            "missing OpenUSD class for SceneIR prototype: " + source.prototype_path);
    }
    if (is_compiler_internal_path(source.material_path)) {
        throw std::invalid_argument(
            "USD-04 requires an authored material for every exported surface: " + source.path);
    }

    const pxr::UsdPrim prim = stage->DefinePrim(pxr::SdfPath {source.path});
    require_authored(prim.GetReferences().AddInternalReference(pxr::SdfPath {prototype->second}),
        "failed to author OpenUSD geometry reference on " + source.path);
    require_authored(prim.SetInstanceable(true),
        "failed to mark OpenUSD geometry instance on " + source.path);
    author_common_prim(source, prim);

    const pxr::UsdShadeMaterial material {
        stage->GetPrimAtPath(pxr::SdfPath {source.material_path})};
    if (!material) {
        throw std::invalid_argument("surface material was not exported: " + source.material_path);
    }
    const pxr::UsdShadeMaterialBindingAPI binding = pxr::UsdShadeMaterialBindingAPI::Apply(prim);
    require_authored(binding && binding.Bind(material),
        "failed to bind OpenUSD material on " + source.path);
}

} // namespace
#endif

void export_openusd_stage(const SceneIRv2& scene, const std::filesystem::path& path) {
#if RT_HAS_OPENUSD
    require_valid_scene_ir_v2(scene);
    if (path.extension() != ".usda") {
        throw std::invalid_argument(
            "deterministic OpenUSD export requires a .usda destination: " + path.string());
    }
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateNew(path.string());
    if (!stage) {
        throw std::runtime_error("failed to create OpenUSD stage: " + path.string());
    }

    const SceneStageMetadata& metadata = scene.stage_metadata();
    require_authored(pxr::UsdGeomSetStageMetersPerUnit(stage, metadata.meters_per_unit),
        "failed to author OpenUSD metersPerUnit");
    require_authored(pxr::UsdGeomSetStageUpAxis(stage, metadata.up_axis == SceneUpAxis::z
                                                           ? pxr::UsdGeomTokens->z
                                                           : pxr::UsdGeomTokens->y),
        "failed to author OpenUSD upAxis");
    stage->SetTimeCodesPerSecond(metadata.time_codes_per_second);
    stage->SetFramesPerSecond(metadata.frames_per_second);
    if (metadata.start_time_code) {
        stage->SetStartTimeCode(*metadata.start_time_code);
    }
    if (metadata.end_time_code) {
        stage->SetEndTimeCode(*metadata.end_time_code);
    }
    stage->SetInterpolationType(metadata.interpolation == SceneTimeInterpolation::held
                                    ? pxr::UsdInterpolationTypeHeld
                                    : pxr::UsdInterpolationTypeLinear);

    const std::vector<const ScenePrim*> prims = ordered_prims(scene);
    for (const ScenePrim* prim : prims) {
        if (is_compiler_internal_path(prim->path) || prim->kind == ScenePrimKind::surface
            || prim->kind == ScenePrimKind::geometry_prototype) {
            continue;
        }
        pxr::UsdPrim authored;
        switch (prim->kind) {
            case ScenePrimKind::scope:
                authored = pxr::UsdGeomXform::Define(stage, pxr::SdfPath {prim->path}).GetPrim();
                author_common_prim(*prim, authored);
                break;
            case ScenePrimKind::material: author_openpbr_material(*prim, stage); break;
            case ScenePrimKind::camera:
                authored = author_camera(*prim, stage);
                author_common_prim(*prim, authored);
                break;
            case ScenePrimKind::light:
                authored = author_light(*prim, stage);
                author_common_prim(*prim, authored);
                break;
            case ScenePrimKind::texture:
                throw std::invalid_argument(
                    "USD-04 standalone texture graph export remains fail-closed: " + prim->path);
            case ScenePrimKind::volume:
                throw std::invalid_argument(
                    "USD-04 volume export remains fail-closed: " + prim->path);
            case ScenePrimKind::surface:
            case ScenePrimKind::geometry_prototype: break;
        }
    }

    std::unordered_map<std::string, std::string> class_path_by_prototype;
    std::unordered_map<std::string, std::string> prototype_by_class_path;
    for (const ScenePrim* prim : prims) {
        if (prim->kind != ScenePrimKind::geometry_prototype) {
            continue;
        }
        const std::string class_path = prototype_class_path(prim->path);
        const auto collision = prototype_by_class_path.emplace(class_path, prim->path);
        if (!collision.second && collision.first->second != prim->path) {
            throw std::invalid_argument(
                "OpenUSD export prototype hash collision for " + prim->path);
        }
        class_path_by_prototype.emplace(prim->path, class_path);
        author_geometry_class(*prim, class_path, stage);
    }

    for (const ScenePrim* prim : prims) {
        if (prim->kind == ScenePrimKind::surface && !is_compiler_internal_path(prim->path)) {
            author_surface(*prim, stage, class_path_by_prototype);
        }
    }

    const pxr::UsdPrim default_prim =
        stage->GetPrimAtPath(pxr::SdfPath {metadata.default_prim_path});
    if (!default_prim || !default_prim.GetPath().IsRootPrimPath()) {
        throw std::invalid_argument(
            "SceneIR default prim is not exportable as a root OpenUSD prim: "
            + metadata.default_prim_path);
    }
    stage->SetDefaultPrim(default_prim);
    require_authored(stage->GetRootLayer()->Save(true),
        "failed to save deterministic OpenUSD stage: " + path.string());
#else
    static_cast<void>(scene);
    static_cast<void>(path);
    throw std::runtime_error(
        "OpenUSD stage export is unavailable; configure with RT_ENABLE_OPENUSD=ON");
#endif
}

} // namespace rt::scene
