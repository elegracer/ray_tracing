#include "scene/openusd_stage_importer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
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
#include <pxr/base/gf/vec2d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/types.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/types.h>
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
#include <pxr/usd/usdGeom/primvar.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/subset.h>
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
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/output.h>
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
constexpr std::string_view kSyntheticTextureRoot = "/__SceneIR/Textures";
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

std::string stable_texture_path(std::string_view source_key) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : source_key) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream path;
    path << kSyntheticTextureRoot << "/T_" << std::hex << std::setw(16) << std::setfill('0')
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

ScenePrimvarInterpolation primvar_interpolation(const pxr::TfToken& value,
    const std::string& prim_path) {
    if (value == pxr::UsdGeomTokens->constant) {
        return ScenePrimvarInterpolation::constant;
    }
    if (value == pxr::UsdGeomTokens->uniform) {
        return ScenePrimvarInterpolation::uniform;
    }
    if (value == pxr::UsdGeomTokens->varying) {
        return ScenePrimvarInterpolation::varying;
    }
    if (value == pxr::UsdGeomTokens->vertex) {
        return ScenePrimvarInterpolation::vertex;
    }
    if (value == pxr::UsdGeomTokens->faceVarying) {
        return ScenePrimvarInterpolation::face_varying;
    }
    throw std::invalid_argument(
        "unsupported OpenUSD primvar interpolation '" + value.GetString() + "' on " + prim_path);
}

template<typename VtArray, typename OutputValue, typename Convert>
std::vector<OutputValue> read_primvar_values(const pxr::UsdGeomPrimvar& primvar,
    const std::string& prim_path, Convert&& convert) {
    VtArray values;
    if (!primvar.Get(&values, pxr::UsdTimeCode::Default())) {
        throw std::invalid_argument("failed to read OpenUSD primvar values on " + prim_path);
    }
    std::vector<OutputValue> result;
    result.reserve(values.size());
    for (const auto& value : values) {
        result.push_back(convert(value));
    }
    return result;
}

ScenePrimvar import_primvar(const pxr::UsdGeomPrimvar& usd_primvar) {
    const std::string prim_path = usd_primvar.GetAttr().GetPath().GetString();
    ScenePrimvar primvar;
    primvar.name = usd_primvar.GetPrimvarName().GetString();
    primvar.interpolation = primvar_interpolation(usd_primvar.GetInterpolation(), prim_path);
    primvar.element_size = static_cast<std::size_t>(std::max(1, usd_primvar.GetElementSize()));

    const pxr::SdfValueTypeName type = usd_primvar.GetTypeName();
    if (type == pxr::SdfValueTypeNames->TexCoord2fArray
        || type == pxr::SdfValueTypeNames->Float2Array) {
        primvar.role = type == pxr::SdfValueTypeNames->TexCoord2fArray ? ScenePrimvarRole::texcoord
                                                                       : ScenePrimvarRole::none;
        primvar.values =
            read_primvar_values<pxr::VtVec2fArray, Eigen::Vector2f>(usd_primvar, prim_path,
                [](const pxr::GfVec2f& value) { return Eigen::Vector2f {value[0], value[1]}; });
    } else if (type == pxr::SdfValueTypeNames->Color3fArray
               || type == pxr::SdfValueTypeNames->Normal3fArray
               || type == pxr::SdfValueTypeNames->Vector3fArray
               || type == pxr::SdfValueTypeNames->Point3fArray
               || type == pxr::SdfValueTypeNames->Float3Array) {
        if (type == pxr::SdfValueTypeNames->Color3fArray) {
            primvar.role = ScenePrimvarRole::color;
        } else if (type == pxr::SdfValueTypeNames->Normal3fArray) {
            primvar.role = ScenePrimvarRole::normal;
        } else if (type == pxr::SdfValueTypeNames->Vector3fArray) {
            primvar.role = ScenePrimvarRole::vector;
        } else if (type == pxr::SdfValueTypeNames->Point3fArray) {
            primvar.role = ScenePrimvarRole::point;
        }
        primvar.values = read_primvar_values<pxr::VtVec3fArray, Eigen::Vector3f>(usd_primvar,
            prim_path, [](const pxr::GfVec3f& value) {
                return Eigen::Vector3f {value[0], value[1], value[2]};
            });
    } else if (type == pxr::SdfValueTypeNames->FloatArray) {
        primvar.values = read_primvar_values<pxr::VtFloatArray, float>(usd_primvar, prim_path,
            [](float value) { return value; });
    } else if (type == pxr::SdfValueTypeNames->DoubleArray) {
        primvar.values = read_primvar_values<pxr::VtDoubleArray, double>(usd_primvar, prim_path,
            [](double value) { return value; });
    } else if (type == pxr::SdfValueTypeNames->IntArray) {
        primvar.values = read_primvar_values<pxr::VtIntArray, std::int32_t>(usd_primvar, prim_path,
            [](int value) { return static_cast<std::int32_t>(value); });
    } else {
        throw std::invalid_argument("unsupported OpenUSD primvar type '"
                                    + type.GetAsToken().GetString() + "' on " + prim_path);
    }

    if (primvar.role == ScenePrimvarRole::none && primvar.name == "normals"
        && std::holds_alternative<std::vector<Eigen::Vector3f>>(primvar.values)) {
        primvar.role = ScenePrimvarRole::normal;
    } else if (primvar.role == ScenePrimvarRole::none && primvar.name == "tangents"
               && std::holds_alternative<std::vector<Eigen::Vector3f>>(primvar.values)) {
        primvar.role = ScenePrimvarRole::vector;
    }

    if (usd_primvar.IsIndexed()) {
        pxr::VtIntArray indices;
        if (!usd_primvar.GetIndices(&indices, pxr::UsdTimeCode::Default())) {
            throw std::invalid_argument("failed to read OpenUSD primvar indices on " + prim_path);
        }
        primvar.indices.assign(indices.begin(), indices.end());
    }
    return primvar;
}

void import_mesh_primvars(const pxr::UsdGeomMesh& mesh, SceneMeshGeometry& geometry) {
    const pxr::UsdGeomPrimvarsAPI primvars_api {mesh.GetPrim()};
    for (const pxr::UsdGeomPrimvar& primvar : primvars_api.GetPrimvarsWithValues()) {
        geometry.primvars.push_back(import_primvar(primvar));
    }
    std::sort(geometry.primvars.begin(), geometry.primvars.end(),
        [](const ScenePrimvar& left, const ScenePrimvar& right) { return left.name < right.name; });
}

void import_material_subsets(const pxr::UsdGeomMesh& mesh, SceneMeshGeometry& geometry) {
    pxr::UsdShadeMaterialBindingAPI binding_api {mesh.GetPrim()};
    const std::vector<pxr::UsdGeomSubset> subsets = binding_api.GetMaterialBindSubsets();
    if (subsets.empty()) {
        return;
    }

    const pxr::TfToken family_type = binding_api.GetMaterialBindSubsetsFamilyType();
    if (family_type == pxr::UsdGeomTokens->partition) {
        geometry.material_subset_family_type = SceneMaterialSubsetFamilyType::partition;
    } else if (family_type != pxr::UsdGeomTokens->nonOverlapping) {
        throw std::invalid_argument("unsupported materialBind subset family type '"
                                    + family_type.GetString() + "' on "
                                    + mesh.GetPath().GetString());
    }

    geometry.material_subsets.reserve(subsets.size());
    for (const pxr::UsdGeomSubset& subset : subsets) {
        const std::string subset_path = subset.GetPath().GetString();
        const pxr::TfToken element_type = read_attribute<pxr::TfToken>(subset.GetElementTypeAttr(),
            subset_path, "GeomSubset element type");
        if (element_type != pxr::UsdGeomTokens->face) {
            throw std::invalid_argument(
                "materialBind GeomSubset must target faces on " + subset_path);
        }

        const pxr::VtIntArray indices = read_attribute<pxr::VtIntArray>(subset.GetIndicesAttr(),
            subset_path, "GeomSubset indices");
        const pxr::UsdShadeMaterial material =
            pxr::UsdShadeMaterialBindingAPI {subset.GetPrim()}.ComputeBoundMaterial();
        if (!material) {
            throw std::invalid_argument(
                "materialBind GeomSubset has no resolved material on " + subset_path);
        }

        SceneMaterialSubset imported;
        imported.name = subset.GetPrim().GetName().GetString();
        imported.face_indices.assign(indices.begin(), indices.end());
        imported.material_path = material.GetPath().GetString();
        geometry.material_subsets.push_back(std::move(imported));
    }
    std::sort(geometry.material_subsets.begin(), geometry.material_subsets.end(),
        [](const SceneMaterialSubset& left, const SceneMaterialSubset& right) {
            return left.name < right.name;
        });
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
    import_mesh_primvars(mesh, geometry);
    import_material_subsets(mesh, geometry);
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

Eigen::Vector2d material_vector2(const pxr::VtValue& value, const std::string& input_path) {
    if (value.IsHolding<pxr::GfVec2f>()) {
        const pxr::GfVec2f& vector = value.UncheckedGet<pxr::GfVec2f>();
        return {static_cast<double>(vector[0]), static_cast<double>(vector[1])};
    }
    if (value.IsHolding<pxr::GfVec2d>()) {
        const pxr::GfVec2d& vector = value.UncheckedGet<pxr::GfVec2d>();
        return {vector[0], vector[1]};
    }
    throw std::invalid_argument(
        "MaterialX vector input requires vector2f or vector2d: " + input_path);
}

std::string material_string(const pxr::VtValue& value, const std::string& input_path) {
    if (value.IsHolding<std::string>()) {
        return value.UncheckedGet<std::string>();
    }
    throw std::invalid_argument("MaterialX string input requires an Sdf string: " + input_path);
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

template<std::size_t N>
void require_supported_inputs(const pxr::UsdShadeShader& shader,
    const std::array<std::string_view, N>& supported_names) {
    for (const pxr::UsdShadeInput& input : shader.GetInputs(true)) {
        const std::string input_name = input.GetBaseName().GetString();
        if (std::find(supported_names.begin(), supported_names.end(), input_name)
            == supported_names.end()) {
            throw std::invalid_argument("unsupported authored MaterialX input on "
                                        + shader.GetPath().GetString() + ".inputs:" + input_name);
        }
    }
}

pxr::VtValue read_materialx_input(const pxr::UsdShadeShader& shader, std::string_view name,
    pxr::VtValue default_value) {
    const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken {std::string {name}});
    if (!input) {
        return default_value;
    }
    pxr::SdfPathVector invalid_sources;
    const pxr::UsdShadeSourceInfoVector sources = input.GetConnectedSources(&invalid_sources);
    if (!invalid_sources.empty() || !sources.empty()) {
        throw std::invalid_argument(
            "MaterialX constant input may not be connected in the "
            "supported subset: "
            + input.GetAttr().GetPath().GetString());
    }
    pxr::VtValue value;
    if (!input.Get(&value, pxr::UsdTimeCode::Default())) {
        throw std::invalid_argument(
            "failed to read authored MaterialX input: " + input.GetAttr().GetPath().GetString());
    }
    return value;
}

void require_unset_materialx_input(const pxr::UsdShadeShader& shader, std::string_view name) {
    const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken {std::string {name}});
    if (input) {
        throw std::invalid_argument("MaterialX input is outside the supported SceneIR subset: "
                                    + input.GetAttr().GetPath().GetString());
    }
}

SceneTextureAddressMode texture_address_mode(std::string_view value,
    const std::string& input_path) {
    if (value == "constant") {
        return SceneTextureAddressMode::constant;
    }
    if (value == "clamp") {
        return SceneTextureAddressMode::clamp;
    }
    if (value == "periodic") {
        return SceneTextureAddressMode::periodic;
    }
    if (value == "mirror") {
        return SceneTextureAddressMode::mirror;
    }
    throw std::invalid_argument(
        "unsupported MaterialX address mode '" + std::string {value} + "' on " + input_path);
}

SceneTextureFilterType texture_filter_type(std::string_view value, const std::string& input_path) {
    if (value == "closest") {
        return SceneTextureFilterType::closest;
    }
    if (value == "linear") {
        return SceneTextureFilterType::linear;
    }
    if (value == "cubic") {
        return SceneTextureFilterType::cubic;
    }
    throw std::invalid_argument(
        "unsupported MaterialX filter type '" + std::string {value} + "' on " + input_path);
}

SceneColorSpace texture_color_space(const pxr::UsdAttribute& file_attribute,
    const std::string& input_path) {
    if (!file_attribute.HasColorSpace()) {
        throw std::invalid_argument(
            "MaterialX image input requires explicit colorSpace metadata: " + input_path);
    }
    const std::string value = file_attribute.GetColorSpace().GetString();
    if (value == "raw") {
        return SceneColorSpace::raw;
    }
    if (value == "lin_rec709" || value == "linear_srgb") {
        return SceneColorSpace::linear_srgb;
    }
    if (value == "sRGB" || value == "srgb_texture") {
        return SceneColorSpace::srgb_texture;
    }
    if (value == "acescg") {
        return SceneColorSpace::acescg;
    }
    throw std::invalid_argument(
        "unsupported MaterialX image color space '" + value + "' on " + input_path);
}

struct ImportedTexture {
    std::string source_key;
    ScenePrim prim;
};

struct MaterialGraphImport {
    std::vector<ImportedTexture> textures;
    std::unordered_map<std::string, std::string> texture_path_by_source;
    std::unordered_map<std::string, std::string> source_by_texture_path;
    std::unordered_set<std::string> active_sources;
};

std::string register_texture_source(std::string source_key, ScenePrim prim,
    MaterialGraphImport& graph) {
    const auto existing = graph.texture_path_by_source.find(source_key);
    if (existing != graph.texture_path_by_source.end()) {
        return existing->second;
    }
    prim.path = stable_texture_path(source_key);
    const auto collision = graph.source_by_texture_path.emplace(prim.path, source_key);
    if (!collision.second && collision.first->second != source_key) {
        throw std::invalid_argument("stable SceneIR texture hash collision for " + source_key);
    }
    const std::string path = prim.path;
    graph.texture_path_by_source.emplace(source_key, path);
    graph.textures.push_back({std::move(source_key), std::move(prim)});
    return path;
}

std::optional<pxr::UsdShadeShader> connected_texture_shader(const pxr::UsdShadeInput& input) {
    pxr::SdfPathVector invalid_sources;
    const pxr::UsdShadeSourceInfoVector sources = input.GetConnectedSources(&invalid_sources);
    const std::string input_path = input.GetAttr().GetPath().GetString();
    if (!invalid_sources.empty()) {
        throw std::invalid_argument("invalid UsdShade connection source on " + input_path);
    }
    if (sources.empty()) {
        return std::nullopt;
    }
    if (sources.size() != 1) {
        throw std::invalid_argument(
            "multiple UsdShade connection sources are unsupported on " + input_path);
    }
    const pxr::UsdShadeConnectionSourceInfo& source = sources.front();
    if (source.sourceType != pxr::UsdShadeAttributeType::Output
        || (source.sourceName != pxr::TfToken {"out"} && source.sourceName != pxr::TfToken {"rgb"})
        || (source.typeName != pxr::SdfValueTypeNames->Color3f
            && source.typeName != pxr::SdfValueTypeNames->Color3d
            && source.typeName != pxr::SdfValueTypeNames->Float3
            && source.typeName != pxr::SdfValueTypeNames->Vector3f)) {
        throw std::invalid_argument(
            "connected color input requires one outputs:out or outputs:rgb source: " + input_path);
    }
    const pxr::UsdShadeShader shader {source.source.GetPrim()};
    if (!shader) {
        throw std::invalid_argument(
            "connected MaterialX source must be a UsdShadeShader: " + input_path);
    }
    if (input.GetAttr().HasAuthoredValueOpinion()) {
        throw std::invalid_argument(
            "connected MaterialX input fallback values are unsupported: " + input_path);
    }
    return shader;
}

std::string import_texture_shader(const pxr::UsdShadeShader& shader, MaterialGraphImport& graph);

std::string import_checker_color(const pxr::UsdShadeShader& shader, std::string_view input_name,
    const Eigen::Vector3d& default_value, MaterialGraphImport& graph) {
    const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken {std::string {input_name}});
    if (input) {
        if (const std::optional<pxr::UsdShadeShader> source = connected_texture_shader(input)) {
            return import_texture_shader(*source, graph);
        }
    }

    Eigen::Vector3d value = default_value;
    if (input) {
        pxr::VtValue authored;
        if (!input.Get(&authored, pxr::UsdTimeCode::Default())) {
            throw std::invalid_argument(
                "failed to read checkerboard color: " + input.GetAttr().GetPath().GetString());
        }
        value = material_color(authored, input.GetAttr().GetPath().GetString());
    }
    ScenePrim constant;
    constant.kind = ScenePrimKind::texture;
    constant.texture = SceneTexture {
        .node = SceneTextureNode::constant_color,
        .node_definition = "ND_constant_color3",
        .color_space = SceneColorSpace::linear_srgb,
        .value = value,
    };
    return register_texture_source(shader.GetPath().GetString()
                                       + ".inputs:" + std::string {input_name},
        std::move(constant), graph);
}

std::string import_texture_shader(const pxr::UsdShadeShader& shader, MaterialGraphImport& graph) {
    const std::string source_key = shader.GetPath().GetString();
    if (graph.active_sources.find(source_key) != graph.active_sources.end()) {
        throw std::invalid_argument("cyclic MaterialX texture graph at " + source_key);
    }
    const auto existing = graph.texture_path_by_source.find(source_key);
    if (existing != graph.texture_path_by_source.end()) {
        return existing->second;
    }
    graph.active_sources.insert(source_key);

    const pxr::TfToken shader_id =
        read_attribute<pxr::TfToken>(shader.GetIdAttr(), source_key, "shader info:id");
    ScenePrim prim;
    prim.kind = ScenePrimKind::texture;
    SceneTexture texture;
    texture.color_space = SceneColorSpace::linear_srgb;

    if (shader_id == pxr::TfToken {"UsdUVTexture"}) {
        require_supported_inputs(shader, std::array<std::string_view, 1> {"file"});
        const pxr::UsdShadeInput file = shader.GetInput(pxr::TfToken {"file"});
        if (!file) {
            throw std::invalid_argument("UsdUVTexture requires inputs:file on " + source_key);
        }
        prim.asset_references.clear();
        append_asset_reference(file.GetAttr(), source_key, prim.asset_references);
        if (prim.asset_references.size() != 1) {
            throw std::invalid_argument(
                "UsdUVTexture requires one authored asset on " + source_key);
        }
        texture.node = SceneTextureNode::image;
        texture.node_definition = "ND_image_color3";
        texture.color_space = SceneColorSpace::srgb_texture;
    } else if (shader_id == pxr::TfToken {"ND_constant_color3"}) {
        require_supported_inputs(shader, std::array<std::string_view, 1> {"value"});
        texture.node = SceneTextureNode::constant_color;
        texture.node_definition = "ND_constant_color3";
        texture.value = material_color(
            read_materialx_input(shader, "value", pxr::VtValue {pxr::GfVec3f {0.0F, 0.0F, 0.0F}}),
            source_key + ".inputs:value");
    } else if (shader_id == pxr::TfToken {"ND_image_color3"}) {
        require_supported_inputs(shader,
            std::array<std::string_view, 10> {"file", "layer", "default", "texcoord",
                "uaddressmode", "vaddressmode", "filtertype", "framerange", "frameoffset",
                "frameendaction"});
        require_unset_materialx_input(shader, "layer");
        require_unset_materialx_input(shader, "texcoord");
        require_unset_materialx_input(shader, "framerange");
        require_unset_materialx_input(shader, "frameoffset");
        require_unset_materialx_input(shader, "frameendaction");

        const pxr::UsdShadeInput file = shader.GetInput(pxr::TfToken {"file"});
        if (!file) {
            throw std::invalid_argument("MaterialX image requires inputs:file on " + source_key);
        }
        if (connected_texture_shader(file)) {
            throw std::invalid_argument(
                "MaterialX image file input may not be connected: " + source_key);
        }
        prim.asset_references.clear();
        append_asset_reference(file.GetAttr(), source_key, prim.asset_references);
        if (prim.asset_references.size() != 1) {
            throw std::invalid_argument(
                "MaterialX image requires one authored asset on " + source_key);
        }

        texture.node = SceneTextureNode::image;
        texture.node_definition = "ND_image_color3";
        texture.color_space =
            texture_color_space(file.GetAttr(), file.GetAttr().GetPath().GetString());
        texture.value = material_color(
            read_materialx_input(shader, "default", pxr::VtValue {pxr::GfVec3f {0.0F, 0.0F, 0.0F}}),
            source_key + ".inputs:default");
        const std::string u_address = material_string(
            read_materialx_input(shader, "uaddressmode", pxr::VtValue {std::string {"periodic"}}),
            source_key + ".inputs:uaddressmode");
        const std::string v_address = material_string(
            read_materialx_input(shader, "vaddressmode", pxr::VtValue {std::string {"periodic"}}),
            source_key + ".inputs:vaddressmode");
        const std::string filter = material_string(
            read_materialx_input(shader, "filtertype", pxr::VtValue {std::string {"linear"}}),
            source_key + ".inputs:filtertype");
        texture.u_address_mode =
            texture_address_mode(u_address, source_key + ".inputs:uaddressmode");
        texture.v_address_mode =
            texture_address_mode(v_address, source_key + ".inputs:vaddressmode");
        texture.filter_type = texture_filter_type(filter, source_key + ".inputs:filtertype");
    } else if (shader_id == pxr::TfToken {"ND_checkerboard_color3"}) {
        require_supported_inputs(shader, std::array<std::string_view, 5> {"color1", "color2",
                                             "uvtiling", "uvoffset", "texcoord"});
        require_unset_materialx_input(shader, "texcoord");
        const Eigen::Vector2d uv_tiling = material_vector2(
            read_materialx_input(shader, "uvtiling", pxr::VtValue {pxr::GfVec2f {8.0F, 8.0F}}),
            source_key + ".inputs:uvtiling");
        const Eigen::Vector2d uv_offset = material_vector2(
            read_materialx_input(shader, "uvoffset", pxr::VtValue {pxr::GfVec2f {0.0F, 0.0F}}),
            source_key + ".inputs:uvoffset");
        if (std::abs(uv_tiling.x() - uv_tiling.y()) > 1e-12 || !uv_offset.isZero(1e-12)) {
            throw std::invalid_argument(
                "SceneIR checkerboard requires uniform UV tiling and zero "
                "offset on "
                + source_key);
        }
        texture.node = SceneTextureNode::checkerboard;
        texture.node_definition = "ND_checkerboard_color3";
        texture.scale = uv_tiling.x();
        texture.even_texture_path =
            import_checker_color(shader, "color1", Eigen::Vector3d::Ones(), graph);
        texture.odd_texture_path =
            import_checker_color(shader, "color2", Eigen::Vector3d::Zero(), graph);
    } else if (shader_id == pxr::TfToken {"ND_noise3d_color3"}) {
        require_supported_inputs(shader,
            std::array<std::string_view, 3> {"amplitude", "pivot", "position"});
        require_unset_materialx_input(shader, "position");
        const Eigen::Vector3d amplitude =
            material_color(read_materialx_input(shader, "amplitude",
                               pxr::VtValue {pxr::GfVec3f {1.0F, 1.0F, 1.0F}}),
                source_key + ".inputs:amplitude");
        const double pivot =
            material_scalar(read_materialx_input(shader, "pivot", pxr::VtValue {0.0F}),
                source_key + ".inputs:pivot");
        if (!amplitude.isApprox(Eigen::Vector3d::Ones(), 1e-12) || pivot != 0.0) {
            throw std::invalid_argument(
                "SceneIR noise3d supports only default amplitude and pivot on " + source_key);
        }
        texture.node = SceneTextureNode::noise3d;
        texture.node_definition = "ND_noise3d_color3";
    } else {
        throw std::invalid_argument(
            "connected OpenPBR inputs use unsupported connected MaterialX "
            "shader '"
            + shader_id.GetString() + "' on " + source_key);
    }

    prim.texture = std::move(texture);
    graph.active_sources.erase(source_key);
    return register_texture_source(source_key, std::move(prim), graph);
}

SceneOpenPbrSurface import_openpbr_surface(const pxr::UsdShadeShader& shader,
    MaterialGraphImport& graph) {
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
        const std::optional<pxr::UsdShadeShader> connected = connected_texture_shader(input);
        if (connected) {
            const auto color_input = std::find_if(color_inputs.begin(), color_inputs.end(),
                [&](const auto& entry) { return entry.first == input_name; });
            if (color_input == color_inputs.end()) {
                throw std::invalid_argument(
                    "connected OpenPBR input requires a supported color3 parameter: " + input_path);
            }
            surface.connections.push_back({input_name, SceneMaterialValueType::color3,
                import_texture_shader(*connected, graph), SceneTextureChannel::rgb});
            continue;
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
    std::sort(surface.connections.begin(), surface.connections.end(),
        [](const SceneMaterialConnection& left, const SceneMaterialConnection& right) {
            return left.input_name != right.input_name ? left.input_name < right.input_name
                                                       : left.texture_path < right.texture_path;
        });
    return surface;
}

SceneOpenPbrSurface import_usd_preview_surface(const pxr::UsdShadeShader& shader,
    MaterialGraphImport& graph) {
    const std::string shader_path = shader.GetPath().GetString();
    SceneOpenPbrSurface surface;
    surface.base_color = Eigen::Vector3d::Constant(0.18);
    surface.specular_roughness = 0.5;
    surface.specular_ior = 1.5;

    for (const pxr::UsdShadeInput& input : shader.GetInputs(true)) {
        const std::string input_name = input.GetBaseName().GetString();
        const std::string input_path = shader_path + ".inputs:" + input_name;
        if (input_name == "diffuseColor") {
            const std::optional<pxr::UsdShadeShader> connected = connected_texture_shader(input);
            if (connected) {
                surface.connections.push_back({"base_color", SceneMaterialValueType::color3,
                    import_texture_shader(*connected, graph), SceneTextureChannel::rgb});
            } else {
                pxr::VtValue value;
                if (!input.Get(&value, pxr::UsdTimeCode::Default())) {
                    throw std::invalid_argument(
                        "failed to read authored UsdPreviewSurface input: " + input_path);
                }
                surface.base_color = material_color(value, input_path);
            }
        } else if (input_name == "roughness") {
            pxr::VtValue value;
            if (!input.Get(&value, pxr::UsdTimeCode::Default())) {
                throw std::invalid_argument(
                    "failed to read authored UsdPreviewSurface input: " + input_path);
            }
            surface.specular_roughness = material_scalar(value, input_path);
        } else if (input_name == "metallic") {
            pxr::VtValue value;
            if (!input.Get(&value, pxr::UsdTimeCode::Default())) {
                throw std::invalid_argument(
                    "failed to read authored UsdPreviewSurface input: " + input_path);
            }
            surface.base_metalness = material_scalar(value, input_path);
        } else if (input_name == "opacity") {
            pxr::VtValue value;
            if (!input.Get(&value, pxr::UsdTimeCode::Default())) {
                throw std::invalid_argument(
                    "failed to read authored UsdPreviewSurface input: " + input_path);
            }
            surface.geometry_opacity = material_scalar(value, input_path);
        } else if (input_name == "ior") {
            pxr::VtValue value;
            if (!input.Get(&value, pxr::UsdTimeCode::Default())) {
                throw std::invalid_argument(
                    "failed to read authored UsdPreviewSurface input: " + input_path);
            }
            surface.specular_ior = material_scalar(value, input_path);
        } else if (input_name == "clearcoat" || input_name == "clearcoatRoughness") {
            pxr::VtValue value;
            if (!input.Get(&value, pxr::UsdTimeCode::Default())) {
                throw std::invalid_argument(
                    "failed to read authored UsdPreviewSurface input: " + input_path);
            }
            const double scalar = material_scalar(value, input_path);
            if (input_name == "clearcoat") {
                surface.coat_weight = scalar;
            } else {
                surface.coat_roughness = scalar;
            }
        } else {
            throw std::invalid_argument(
                "unsupported authored UsdPreviewSurface input: " + input_path);
        }
    }
    return surface;
}

SceneMaterial import_material(const pxr::UsdShadeMaterial& material, MaterialGraphImport& graph) {
    pxr::TfToken source_name;
    pxr::UsdShadeAttributeType source_type;
    const pxr::UsdShadeShader shader = material.ComputeSurfaceSource(
        pxr::TfTokenVector {pxr::UsdShadeTokens->universalRenderContext}, &source_name,
        &source_type);
    if (!shader) {
        throw std::invalid_argument("OpenUSD material has no resolved universal surface source: "
                                    + material.GetPath().GetString());
    }
    const pxr::TfToken shader_id = read_attribute<pxr::TfToken>(shader.GetIdAttr(),
        shader.GetPath().GetString(), "shader info:id");
    if (shader_id == pxr::TfToken {"UsdPreviewSurface"}) {
        return import_usd_preview_surface(shader, graph);
    }
    return import_openpbr_surface(shader, graph);
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
    std::unordered_map<std::string, std::string> texture_source_by_path;
    pxr::UsdShadeMaterialBindingAPI::BindingsCache bindings_cache;
    pxr::UsdShadeMaterialBindingAPI::CollectionQueryCache collection_query_cache;

    void add(ScenePrim prim) {
        if (!added_paths.insert(prim.path).second) {
            throw std::invalid_argument("duplicate compiled SceneIR v2 prim path: " + prim.path);
        }
        scene.add_prim(std::move(prim));
    }

    void add_texture(ImportedTexture imported) {
        const auto source = texture_source_by_path.emplace(imported.prim.path, imported.source_key);
        if (!source.second) {
            if (source.first->second != imported.source_key) {
                throw std::invalid_argument(
                    "stable SceneIR texture hash collision for " + imported.source_key);
            }
            return;
        }
        add(std::move(imported.prim));
    }
};

void add_synthetic_roots(ImportContext& context, const pxr::UsdStageRefPtr& stage) {
    for (const std::string_view path : {kSyntheticRoot, kSyntheticPrototypeRoot,
             kSyntheticMaterialRoot, kSyntheticTextureRoot, kDefaultMaterialPath}) {
        if (stage->GetPrimAtPath(pxr::SdfPath {std::string {path}})) {
            throw std::invalid_argument(
                "OpenUSD stage uses reserved SceneIR compiler path: " + std::string {path});
        }
    }

    for (const std::string_view path :
        {kSyntheticRoot, kSyntheticPrototypeRoot, kSyntheticMaterialRoot, kSyntheticTextureRoot}) {
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
    if (const pxr::UsdGeomSubset subset {usd_prim}) {
        // Material-binding subsets are compiled into their parent mesh prototype.
        return;
    }

    ScenePrim prim;
    prim.path = prim_path;
    import_xformable(usd_prim, prim);
    import_imageable(usd_prim, prim);

    if (const pxr::UsdShadeMaterial material {usd_prim}) {
        prim.kind = ScenePrimKind::material;
        MaterialGraphImport graph;
        prim.material = import_material(material, graph);
        for (ImportedTexture& texture : graph.textures) {
            context.add_texture(std::move(texture));
        }
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
