#include "scene/openusd_stage_importer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if RT_HAS_OPENUSD
#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/types.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usd/primDefinition.h>
#include <pxr/usd/usd/interpolation.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primFlags.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/tokens.h>
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
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/tokens.h>
#endif

namespace rt::scene {

bool openusd_stage_importer_available() {
#if RT_HAS_OPENUSD
    return true;
#else
    return false;
#endif
}

#if RT_HAS_OPENUSD
namespace {

constexpr std::string_view kSyntheticRoot = "/__SceneIR";
constexpr std::string_view kSyntheticPrototypeRoot = "/__SceneIR/Prototypes";
constexpr std::string_view kSyntheticMaterialRoot = "/__SceneIR/Materials";
constexpr std::string_view kDefaultMaterialPath = "/__SceneIR/Materials/Default";

Eigen::Matrix4d to_eigen_transform(const pxr::GfMatrix4d& value) {
    Eigen::Matrix4d result;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            // Gf applies transforms to row vectors; SceneIR v2 and Eigen use column vectors.
            result(row, column) = value[column][row];
        }
    }
    return result;
}

Eigen::Vector3d to_eigen_vector(const pxr::GfVec3f& value) {
    return {static_cast<double>(value[0]), static_cast<double>(value[1]),
        static_cast<double>(value[2])};
}

Eigen::Vector3d to_eigen_vector(const pxr::GfVec3d& value) {
    return {value[0], value[1], value[2]};
}

template<typename T>
T read_attribute(const pxr::UsdAttribute& attribute, const std::string& prim_path,
    std::string_view attribute_name) {
    T value;
    if (!attribute || !attribute.Get(&value, pxr::UsdTimeCode::Default())) {
        throw std::invalid_argument(
            "failed to read OpenUSD " + std::string {attribute_name} + " on " + prim_path);
    }
    return value;
}

std::string stable_prototype_path(std::string_view anchor_path) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : anchor_path) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream path;
    path << kSyntheticPrototypeRoot << "/P_" << std::hex << std::setw(16) << std::setfill('0')
         << hash;
    return path.str();
}

bool is_supported_geometry(const pxr::UsdPrim& prim) {
    return static_cast<bool>(pxr::UsdGeomSphere {prim})
           || static_cast<bool>(pxr::UsdGeomMesh {prim});
}

std::string geometry_source_key(const pxr::UsdPrim& prim) {
    if (prim.IsInstance()) {
        const pxr::UsdPrim prototype_prim = prim.GetPrototype();
        if (!prototype_prim) {
            throw std::invalid_argument("OpenUSD instance does not resolve to a prototype prim: "
                                        + prim.GetPath().GetString());
        }
        return "prototype:" + prototype_prim.GetPath().GetString();
    }
    if (prim.IsInstanceProxy()) {
        const pxr::UsdPrim prototype_prim = prim.GetPrimInPrototype();
        if (!prototype_prim) {
            throw std::invalid_argument(
                "OpenUSD instance proxy does not resolve to a prototype prim: "
                + prim.GetPath().GetString());
        }
        // Prototype paths are runtime-only keys. They never enter SceneIR because OpenUSD
        // explicitly does not guarantee that generated prototype paths are stable.
        return "prototype:" + prototype_prim.GetPath().GetString();
    }
    return "prim:" + prim.GetPath().GetString();
}

ScenePurpose scene_purpose(const pxr::TfToken& value, const std::string& prim_path) {
    if (value == pxr::UsdGeomTokens->default_) {
        return ScenePurpose::default_;
    }
    if (value == pxr::UsdGeomTokens->render) {
        return ScenePurpose::render;
    }
    if (value == pxr::UsdGeomTokens->proxy) {
        return ScenePurpose::proxy;
    }
    if (value == pxr::UsdGeomTokens->guide) {
        return ScenePurpose::guide;
    }
    throw std::invalid_argument(
        "unsupported OpenUSD purpose '" + value.GetString() + "' on " + prim_path);
}

void import_imageable(const pxr::UsdPrim& usd_prim, ScenePrim& prim) {
    const pxr::UsdGeomImageable imageable {usd_prim};
    if (!imageable) {
        return;
    }

    const pxr::UsdAttribute visibility = imageable.GetVisibilityAttr();
    if (visibility.HasAuthoredValueOpinion()) {
        pxr::TfToken value;
        if (!visibility.Get(&value)) {
            throw std::invalid_argument("failed to read OpenUSD visibility on " + prim.path);
        }
        if (value == pxr::UsdGeomTokens->invisible) {
            prim.visibility = SceneVisibility::invisible;
        } else if (value != pxr::UsdGeomTokens->inherited) {
            throw std::invalid_argument(
                "unsupported OpenUSD visibility '" + value.GetString() + "' on " + prim.path);
        }
    }

    const pxr::UsdAttribute purpose = imageable.GetPurposeAttr();
    if (purpose.HasAuthoredValueOpinion()) {
        pxr::TfToken value;
        if (!purpose.Get(&value)) {
            throw std::invalid_argument("failed to read OpenUSD purpose on " + prim.path);
        }
        prim.authored_purpose = scene_purpose(value, prim.path);
    }
}

void import_xformable(const pxr::UsdPrim& usd_prim, ScenePrim& prim) {
    const pxr::UsdGeomXformable xformable {usd_prim};
    if (!xformable) {
        return;
    }

    pxr::GfMatrix4d local_transform;
    bool resets_xform_stack = false;
    if (!xformable.GetLocalTransformation(&local_transform, &resets_xform_stack,
            pxr::UsdTimeCode::Default())) {
        throw std::invalid_argument("failed to read OpenUSD local transform on " + prim.path);
    }
    prim.local_to_parent = to_eigen_transform(local_transform);
    prim.reset_xform_stack = resets_xform_stack;

    std::vector<double> time_samples;
    if (!xformable.GetTimeSamples(&time_samples)) {
        throw std::invalid_argument("failed to read OpenUSD transform samples on " + prim.path);
    }
    prim.transform_samples.reserve(time_samples.size());
    for (const double time_code : time_samples) {
        if (!xformable.GetLocalTransformation(&local_transform, &resets_xform_stack,
                pxr::UsdTimeCode {time_code})) {
            throw std::invalid_argument(
                "failed to evaluate OpenUSD transform sample on " + prim.path);
        }
        SceneTransformSample sample;
        sample.time_code = time_code;
        sample.local_to_parent = to_eigen_transform(local_transform);
        prim.transform_samples.push_back(std::move(sample));
    }
}

SceneGeometry import_geometry(const pxr::UsdPrim& usd_prim) {
    const std::string prim_path = usd_prim.GetPath().GetString();
    if (const pxr::UsdGeomSphere sphere {usd_prim}) {
        SceneSphereGeometry geometry;
        geometry.radius =
            read_attribute<double>(sphere.GetRadiusAttr(), prim_path, "sphere radius");
        return geometry;
    }

    const pxr::UsdGeomMesh mesh {usd_prim};
    if (!mesh) {
        throw std::invalid_argument("unsupported OpenUSD geometry on " + prim_path);
    }

    pxr::VtVec3fArray points;
    pxr::VtIntArray face_vertex_counts;
    pxr::VtIntArray face_vertex_indices;
    if (!mesh.GetPointsAttr().Get(&points)
        || !mesh.GetFaceVertexCountsAttr().Get(&face_vertex_counts)
        || !mesh.GetFaceVertexIndicesAttr().Get(&face_vertex_indices)) {
        throw std::invalid_argument("failed to read OpenUSD mesh topology on " + prim_path);
    }

    SceneMeshGeometry geometry;
    geometry.points.reserve(points.size());
    for (const pxr::GfVec3f& point : points) {
        geometry.points.push_back(to_eigen_vector(point));
    }
    geometry.face_vertex_counts.assign(face_vertex_counts.begin(), face_vertex_counts.end());
    geometry.face_vertex_indices.assign(face_vertex_indices.begin(), face_vertex_indices.end());

    const pxr::TfToken orientation =
        read_attribute<pxr::TfToken>(mesh.GetOrientationAttr(), prim_path, "mesh orientation");
    if (orientation == pxr::UsdGeomTokens->rightHanded) {
        geometry.orientation = SceneMeshOrientation::right_handed;
    } else if (orientation == pxr::UsdGeomTokens->leftHanded) {
        geometry.orientation = SceneMeshOrientation::left_handed;
    } else {
        throw std::invalid_argument("unsupported OpenUSD mesh orientation '"
                                    + orientation.GetString() + "' on " + prim_path);
    }

    const pxr::TfToken subdivision = read_attribute<pxr::TfToken>(mesh.GetSubdivisionSchemeAttr(),
        prim_path, "mesh subdivision scheme");
    if (subdivision == pxr::UsdGeomTokens->none) {
        geometry.subdivision_scheme = SceneSubdivisionScheme::none;
    } else if (subdivision == pxr::UsdGeomTokens->catmullClark) {
        geometry.subdivision_scheme = SceneSubdivisionScheme::catmull_clark;
    } else if (subdivision == pxr::UsdGeomTokens->loop) {
        geometry.subdivision_scheme = SceneSubdivisionScheme::loop;
    } else if (subdivision == pxr::UsdGeomTokens->bilinear) {
        geometry.subdivision_scheme = SceneSubdivisionScheme::bilinear;
    } else {
        throw std::invalid_argument("unsupported OpenUSD mesh subdivision scheme '"
                                    + subdivision.GetString() + "' on " + prim_path);
    }
    return geometry;
}

double material_scalar(const pxr::VtValue& value, const std::string& input_path) {
    if (value.IsHolding<float>()) {
        return static_cast<double>(value.UncheckedGet<float>());
    }
    if (value.IsHolding<double>()) {
        return value.UncheckedGet<double>();
    }
    throw std::invalid_argument("OpenPBR scalar input requires float or double: " + input_path);
}

Eigen::Vector3d material_color(const pxr::VtValue& value, const std::string& input_path) {
    if (value.IsHolding<pxr::GfVec3f>()) {
        return to_eigen_vector(value.UncheckedGet<pxr::GfVec3f>());
    }
    if (value.IsHolding<pxr::GfVec3d>()) {
        return to_eigen_vector(value.UncheckedGet<pxr::GfVec3d>());
    }
    throw std::invalid_argument("OpenPBR color input requires color3f or color3d: " + input_path);
}

SceneOpenPbrSurface import_openpbr_surface(const pxr::UsdShadeShader& shader) {
    const std::string shader_path = shader.GetPath().GetString();
    const pxr::TfToken shader_id =
        read_attribute<pxr::TfToken>(shader.GetIdAttr(), shader_path, "shader info:id");
    if (shader_id.GetString() != kOpenPbrMaterialXNodeDef) {
        throw std::invalid_argument("unsupported OpenUSD surface shader '" + shader_id.GetString()
                                    + "' on " + shader_path
                                    + "; expected ND_open_pbr_surface_surfaceshader");
    }

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

    SceneOpenPbrSurface surface;
    for (const pxr::UsdShadeInput& input : shader.GetInputs(true)) {
        const std::string input_name = input.GetBaseName().GetString();
        const std::string input_path = shader_path + ".inputs:" + input_name;
        if (input.HasConnectedSource()) {
            throw std::invalid_argument(
                "connected OpenPBR inputs are not in the USD-03 supported subset: " + input_path);
        }
        pxr::VtValue value;
        if (!input.Get(&value, pxr::UsdTimeCode::Default())) {
            throw std::invalid_argument("failed to read authored OpenPBR input: " + input_path);
        }

        bool consumed = false;
        for (const auto& entry : scalar_inputs) {
            if (entry.first == input_name) {
                surface.*entry.second = material_scalar(value, input_path);
                consumed = true;
                break;
            }
        }
        if (!consumed) {
            for (const auto& entry : color_inputs) {
                if (entry.first == input_name) {
                    surface.*entry.second = material_color(value, input_path);
                    consumed = true;
                    break;
                }
            }
        }
        if (!consumed && input_name == "geometry_thin_walled") {
            if (!value.IsHolding<bool>()) {
                throw std::invalid_argument(
                    "OpenPBR geometry_thin_walled input requires bool: " + input_path);
            }
            surface.geometry_thin_walled = value.UncheckedGet<bool>();
            consumed = true;
        }
        if (!consumed) {
            throw std::invalid_argument(
                "unsupported authored OpenPBR input in USD-03: " + input_path);
        }
    }
    return surface;
}

SceneMaterial import_material(const pxr::UsdShadeMaterial& material) {
    pxr::TfToken source_name;
    pxr::UsdShadeAttributeType source_type;
    const pxr::UsdShadeShader shader = material.ComputeSurfaceSource(
        pxr::TfTokenVector {pxr::UsdShadeTokens->universalRenderContext}, &source_name,
        &source_type);
    if (!shader) {
        throw std::invalid_argument("OpenUSD material has no resolved universal surface source: "
                                    + material.GetPath().GetString());
    }
    return import_openpbr_surface(shader);
}

SceneCamera import_camera(const pxr::UsdGeomCamera& camera) {
    const pxr::GfCamera value = camera.GetCamera(pxr::UsdTimeCode::Default());
    SceneCamera result;
    result.projection = value.GetProjection() == pxr::GfCamera::Orthographic
                            ? SceneCameraProjection::orthographic
                            : SceneCameraProjection::perspective;
    result.horizontal_aperture = value.GetHorizontalAperture();
    result.vertical_aperture = value.GetVerticalAperture();
    result.horizontal_aperture_offset = value.GetHorizontalApertureOffset();
    result.vertical_aperture_offset = value.GetVerticalApertureOffset();
    result.focal_length = value.GetFocalLength();
    const pxr::GfRange1f clipping = value.GetClippingRange();
    result.clipping_range = {clipping.GetMin(), clipping.GetMax()};
    result.f_stop = value.GetFStop();
    result.focus_distance = value.GetFocusDistance();
    return result;
}

SceneLightMaterialSyncMode light_material_sync_mode(const pxr::TfToken& value,
    const std::string& prim_path) {
    if (value == pxr::UsdLuxTokens->materialGlowTintsLight) {
        return SceneLightMaterialSyncMode::material_glow_tints_light;
    }
    if (value == pxr::UsdLuxTokens->independent) {
        return SceneLightMaterialSyncMode::independent;
    }
    if (value == pxr::UsdLuxTokens->noMaterialResponse) {
        return SceneLightMaterialSyncMode::no_material_response;
    }
    throw std::invalid_argument(
        "unsupported UsdLux materialSyncMode '" + value.GetString() + "' on " + prim_path);
}

void append_asset_reference(const pxr::UsdAttribute& attribute, const std::string& prim_path,
    std::vector<SceneAssetReference>& assets) {
    if (!attribute.HasAuthoredValueOpinion()) {
        return;
    }
    const pxr::SdfAssetPath value =
        read_attribute<pxr::SdfAssetPath>(attribute, prim_path, "asset path");
    SceneAssetReference asset;
    asset.authored_path = value.GetAssetPath();
    asset.evaluated_path = value.GetAssetPath();
    asset.resolved_path = value.GetResolvedPath();
    assets.push_back(std::move(asset));
}

SceneLight import_light(const pxr::UsdPrim& usd_prim, std::vector<SceneAssetReference>& assets) {
    const std::string prim_path = usd_prim.GetPath().GetString();
    const pxr::UsdLuxLightAPI api {usd_prim};
    if (!api) {
        throw std::invalid_argument("OpenUSD light does not apply UsdLuxLightAPI: " + prim_path);
    }

    SceneLight light;
    light.color =
        to_eigen_vector(read_attribute<pxr::GfVec3f>(api.GetColorAttr(), prim_path, "light color"));
    light.intensity = read_attribute<float>(api.GetIntensityAttr(), prim_path, "light intensity");
    light.exposure = read_attribute<float>(api.GetExposureAttr(), prim_path, "light exposure");
    light.normalize = read_attribute<bool>(api.GetNormalizeAttr(), prim_path, "light normalize");
    light.enable_color_temperature = read_attribute<bool>(api.GetEnableColorTemperatureAttr(),
        prim_path, "light color-temperature enable");
    light.color_temperature_kelvin =
        read_attribute<float>(api.GetColorTemperatureAttr(), prim_path, "light color temperature");
    light.diffuse = read_attribute<float>(api.GetDiffuseAttr(), prim_path, "light diffuse");
    light.specular = read_attribute<float>(api.GetSpecularAttr(), prim_path, "light specular");
    light.material_sync_mode =
        light_material_sync_mode(read_attribute<pxr::TfToken>(api.GetMaterialSyncModeAttr(),
                                     prim_path, "light material-sync mode"),
            prim_path);

    if (const pxr::UsdLuxSphereLight sphere {usd_prim}) {
        light.type = SceneLightType::sphere;
        light.radius =
            read_attribute<float>(sphere.GetRadiusAttr(), prim_path, "sphere-light radius");
        light.treat_as_point = read_attribute<bool>(sphere.GetTreatAsPointAttr(), prim_path,
            "sphere-light treatAsPoint");
    } else if (const pxr::UsdLuxDiskLight disk {usd_prim}) {
        light.type = SceneLightType::disk;
        light.radius = read_attribute<float>(disk.GetRadiusAttr(), prim_path, "disk-light radius");
    } else if (const pxr::UsdLuxRectLight rect {usd_prim}) {
        light.type = SceneLightType::rect;
        light.width = read_attribute<float>(rect.GetWidthAttr(), prim_path, "rect-light width");
        light.height = read_attribute<float>(rect.GetHeightAttr(), prim_path, "rect-light height");
        append_asset_reference(rect.GetTextureFileAttr(), prim_path, assets);
    } else if (const pxr::UsdLuxCylinderLight cylinder {usd_prim}) {
        light.type = SceneLightType::cylinder;
        light.radius =
            read_attribute<float>(cylinder.GetRadiusAttr(), prim_path, "cylinder-light radius");
        light.length =
            read_attribute<float>(cylinder.GetLengthAttr(), prim_path, "cylinder-light length");
        light.treat_as_line = read_attribute<bool>(cylinder.GetTreatAsLineAttr(), prim_path,
            "cylinder-light treatAsLine");
    } else if (const pxr::UsdLuxDistantLight distant {usd_prim}) {
        light.type = SceneLightType::distant;
        light.angle_degrees =
            read_attribute<float>(distant.GetAngleAttr(), prim_path, "distant-light angle");
    } else if (const pxr::UsdLuxDomeLight dome {usd_prim}) {
        light.type = SceneLightType::dome;
        append_asset_reference(dome.GetTextureFileAttr(), prim_path, assets);
    } else {
        throw std::invalid_argument("unsupported UsdLux light schema on " + prim_path);
    }
    return light;
}

SceneStageMetadata import_stage_metadata(const pxr::UsdStageRefPtr& stage) {
    SceneStageMetadata metadata;
    metadata.meters_per_unit = pxr::UsdGeomGetStageMetersPerUnit(stage);
    const pxr::TfToken up_axis = pxr::UsdGeomGetStageUpAxis(stage);
    if (up_axis == pxr::UsdGeomTokens->z) {
        metadata.up_axis = SceneUpAxis::z;
    } else if (up_axis == pxr::UsdGeomTokens->y) {
        metadata.up_axis = SceneUpAxis::y;
    } else {
        throw std::invalid_argument("unsupported OpenUSD stage up axis: " + up_axis.GetString());
    }
    metadata.right_handed = true;
    metadata.time_codes_per_second = stage->GetTimeCodesPerSecond();
    metadata.frames_per_second = stage->GetFramesPerSecond();
    if (stage->HasAuthoredTimeCodeRange()) {
        metadata.start_time_code = stage->GetStartTimeCode();
        metadata.end_time_code = stage->GetEndTimeCode();
    }
    metadata.interpolation = stage->GetInterpolationType() == pxr::UsdInterpolationTypeHeld
                                 ? SceneTimeInterpolation::held
                                 : SceneTimeInterpolation::linear;

    const pxr::UsdPrim default_prim = stage->GetDefaultPrim();
    if (!default_prim) {
        throw std::invalid_argument(
            "OpenUSD stage requires a valid defaultPrim for SceneIR v2 import");
    }
    metadata.default_prim_path = default_prim.GetPath().GetString();
    return metadata;
}

struct ImportContext {
    SceneIRv2 scene;
    std::unordered_set<std::string> added_paths;
    std::unordered_map<std::string, std::string> prototype_path_by_source;
    std::unordered_set<std::string> created_prototypes;
    pxr::UsdShadeMaterialBindingAPI::BindingsCache bindings_cache;
    pxr::UsdShadeMaterialBindingAPI::CollectionQueryCache collection_query_cache;

    void add(ScenePrim prim) {
        if (!added_paths.insert(prim.path).second) {
            throw std::invalid_argument("duplicate compiled SceneIR v2 prim path: " + prim.path);
        }
        scene.add_prim(std::move(prim));
    }
};

void add_synthetic_roots(ImportContext& context, const pxr::UsdStageRefPtr& stage) {
    for (const std::string_view path :
        {kSyntheticRoot, kSyntheticPrototypeRoot, kSyntheticMaterialRoot, kDefaultMaterialPath}) {
        if (stage->GetPrimAtPath(pxr::SdfPath {std::string {path}})) {
            throw std::invalid_argument(
                "OpenUSD stage uses reserved SceneIR compiler path: " + std::string {path});
        }
    }

    for (const std::string_view path :
        {kSyntheticRoot, kSyntheticPrototypeRoot, kSyntheticMaterialRoot}) {
        ScenePrim scope;
        scope.path = std::string {path};
        context.add(std::move(scope));
    }
    ScenePrim material;
    material.path = std::string {kDefaultMaterialPath};
    material.kind = ScenePrimKind::material;
    material.material = SceneOpenPbrSurface {};
    context.add(std::move(material));
}

std::vector<pxr::UsdPrim> collect_stage_prims(const pxr::UsdStageRefPtr& stage) {
    std::vector<pxr::UsdPrim> prims;
    for (const pxr::UsdPrim& prim : stage->Traverse(pxr::UsdTraverseInstanceProxies())) {
        prims.push_back(prim);
    }
    return prims;
}

void build_prototype_map(const std::vector<pxr::UsdPrim>& prims, ImportContext& context) {
    std::map<std::string, std::string> anchor_by_source;
    for (const pxr::UsdPrim& prim : prims) {
        if (!is_supported_geometry(prim)) {
            continue;
        }
        const std::string source = geometry_source_key(prim);
        const std::string surface_path = prim.GetPath().GetString();
        const auto inserted = anchor_by_source.emplace(source, surface_path);
        if (!inserted.second && surface_path < inserted.first->second) {
            inserted.first->second = surface_path;
        }
    }
    for (const auto& entry : anchor_by_source) {
        const std::string prototype_path = stable_prototype_path(entry.second);
        const auto collision = std::find_if(context.prototype_path_by_source.begin(),
            context.prototype_path_by_source.end(), [&](const auto& existing) {
                return existing.second == prototype_path && existing.first != entry.first;
            });
        if (collision != context.prototype_path_by_source.end()) {
            throw std::invalid_argument(
                "stable SceneIR prototype hash collision for " + entry.second);
        }
        context.prototype_path_by_source.emplace(entry.first, prototype_path);
    }
}

std::string resolve_material_path(const pxr::UsdPrim& prim, ImportContext& context) {
    const pxr::UsdShadeMaterial material =
        pxr::UsdShadeMaterialBindingAPI {prim}.ComputeBoundMaterial(&context.bindings_cache,
            &context.collection_query_cache, pxr::UsdShadeTokens->allPurpose, nullptr, true);
    return material ? material.GetPath().GetString() : std::string {kDefaultMaterialPath};
}

void import_prim(const pxr::UsdPrim& usd_prim, ImportContext& context) {
    const std::string prim_path = usd_prim.GetPath().GetString();

    if (const pxr::UsdShadeShader shader {usd_prim}) {
        // Shader payloads are owned by their material prim and compiled through the
        // resolved material surface source instead of becoming standalone SceneIR prims.
        return;
    }

    ScenePrim prim;
    prim.path = prim_path;
    import_xformable(usd_prim, prim);
    import_imageable(usd_prim, prim);

    if (const pxr::UsdShadeMaterial material {usd_prim}) {
        prim.kind = ScenePrimKind::material;
        prim.material = import_material(material);
    } else if (const pxr::UsdGeomCamera camera {usd_prim}) {
        prim.kind = ScenePrimKind::camera;
        prim.camera = import_camera(camera);
    } else if (pxr::UsdLuxLightAPI {usd_prim}) {
        prim.kind = ScenePrimKind::light;
        prim.light = import_light(usd_prim, prim.asset_references);
    } else if (is_supported_geometry(usd_prim)) {
        prim.kind = ScenePrimKind::surface;
        const std::string source = geometry_source_key(usd_prim);
        const auto prototype_path = context.prototype_path_by_source.find(source);
        if (prototype_path == context.prototype_path_by_source.end()) {
            throw std::logic_error("missing OpenUSD geometry prototype mapping for " + prim_path);
        }
        prim.prototype_path = prototype_path->second;
        prim.material_path = resolve_material_path(usd_prim, context);
        if (context.created_prototypes.insert(prim.prototype_path).second) {
            ScenePrim prototype;
            prototype.path = prim.prototype_path;
            prototype.kind = ScenePrimKind::geometry_prototype;
            prototype.geometry = import_geometry(usd_prim);
            context.add(std::move(prototype));
        }
    } else {
        const std::string type_name = usd_prim.GetTypeName().GetString();
        if (!type_name.empty() && type_name != "Scope" && type_name != "Xform") {
            throw std::invalid_argument(
                "unsupported OpenUSD prim type '" + type_name + "' on " + prim_path);
        }
    }
    context.add(std::move(prim));
}

} // namespace
#endif

SceneIRv2 import_openusd_stage(const std::filesystem::path& path) {
#if RT_HAS_OPENUSD
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(path.string());
    if (!stage) {
        throw std::invalid_argument("failed to open composed OpenUSD stage: " + path.string());
    }

    ImportContext context;
    context.scene.stage_metadata() = import_stage_metadata(stage);
    add_synthetic_roots(context, stage);
    const std::vector<pxr::UsdPrim> prims = collect_stage_prims(stage);
    build_prototype_map(prims, context);
    for (const pxr::UsdPrim& usd_prim : prims) {
        import_prim(usd_prim, context);
    }
    require_valid_scene_ir_v2(context.scene);
    return std::move(context.scene);
#else
    static_cast<void>(path);
    throw std::runtime_error(
        "OpenUSD stage import is unavailable; configure with RT_ENABLE_OPENUSD=ON");
#endif
}

} // namespace rt::scene
