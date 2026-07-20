#include "scene/materialx_openpbr_loader.h"

#include <MaterialXCore/Document.h>
#include <MaterialXCore/Types.h>
#include <MaterialXFormat/XmlIo.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace rt::scene {
namespace {

namespace mx = MaterialX;

using ScalarMember = double SceneOpenPbrSurface::*;
using ColorMember = Eigen::Vector3d SceneOpenPbrSurface::*;

constexpr std::array<std::pair<std::string_view, ScalarMember>, 27> kScalarInputs {{
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

constexpr std::array<std::pair<std::string_view, ColorMember>, 9> kColorInputs {{
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

std::invalid_argument input_error(const std::filesystem::path& path, const mx::InputPtr& input,
    std::string_view message) {
    return std::invalid_argument(
        path.string() + ": OpenPBR input '" + input->getName() + "' " + std::string {message});
}

void apply_input(SceneOpenPbrSurface& surface, const mx::InputPtr& input,
    const std::filesystem::path& path) {
    if (!input->hasValue()) {
        throw input_error(path, input, "must be a constant value");
    }
    const mx::ValuePtr value = input->getValue();
    if (!value) {
        throw input_error(path, input, "has an invalid typed value");
    }

    const std::string& name = input->getName();
    if (name == "geometry_thin_walled") {
        if (input->getType() != "boolean") {
            throw input_error(path, input, "must have MaterialX boolean type");
        }
        surface.geometry_thin_walled = value->asA<bool>();
        return;
    }
    for (const auto& [candidate, member] : kScalarInputs) {
        if (name == candidate) {
            if (input->getType() != "float") {
                throw input_error(path, input, "must have MaterialX float type");
            }
            surface.*member = static_cast<double>(value->asA<float>());
            return;
        }
    }
    for (const auto& [candidate, member] : kColorInputs) {
        if (name == candidate) {
            if (input->getType() != "color3") {
                throw input_error(path, input, "must have MaterialX color3 type");
            }
            const mx::Color3 color = value->asA<mx::Color3>();
            surface.*member = Eigen::Vector3d {color[0], color[1], color[2]};
            return;
        }
    }
    throw input_error(path, input, "is not supported by OpenPBR 1.1.1");
}

} // namespace

MaterialXOpenPbrDocument load_materialx_openpbr(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) {
        throw std::invalid_argument(
            "MaterialX OpenPBR source is not a regular file: " + path.string());
    }

    const mx::DocumentPtr document = mx::createDocument();
    mx::readFromXmlFile(document, mx::FilePath {path.string()});

    const std::vector<mx::NodePtr> surfaces = document->getNodes("open_pbr_surface");
    const std::vector<mx::NodePtr> materials = document->getNodes("surfacematerial");
    if (surfaces.size() != 1 || materials.size() != 1) {
        throw std::invalid_argument(
            path.string() + ": expected exactly one open_pbr_surface and one surfacematerial");
    }
    const mx::InputPtr shader_input = materials.front()->getInput("surfaceshader");
    if (!shader_input || !shader_input->hasNodeName()
        || shader_input->getNodeName() != surfaces.front()->getName()) {
        throw std::invalid_argument(
            path.string() + ": surfacematerial does not bind the OpenPBR surface node");
    }

    SceneOpenPbrSurface surface;
    for (const mx::InputPtr& input : surfaces.front()->getInputs()) {
        apply_input(surface, input, path);
    }
    return MaterialXOpenPbrDocument {
        .source_path = std::filesystem::canonical(path),
        .material_name = materials.front()->getName(),
        .document_color_space = document->getAttribute("colorspace"),
        .surface = std::move(surface),
    };
}

std::vector<MaterialXOpenPbrDocument> load_materialx_openpbr_directory(
    const std::filesystem::path& directory) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::invalid_argument(
            "MaterialX OpenPBR source is not a directory: " + directory.string());
    }
    std::vector<std::filesystem::path> paths;
    for (const std::filesystem::directory_entry& entry :
        std::filesystem::directory_iterator {directory}) {
        if (entry.is_regular_file() && entry.path().extension() == ".mtlx") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());

    std::vector<MaterialXOpenPbrDocument> result;
    result.reserve(paths.size());
    for (const std::filesystem::path& path : paths) {
        result.push_back(load_materialx_openpbr(path));
    }
    return result;
}

} // namespace rt::scene
