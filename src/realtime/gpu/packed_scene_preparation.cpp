#include "realtime/gpu/packed_scene_preparation.h"

#include <opencv2/opencv.hpp>

#include <array>
#include <cmath>
#include <type_traits>

namespace rt {
namespace {

PackedLightVector3 pack_light_vector(const Eigen::Vector3d& value) {
    return {static_cast<float>(value.x()), static_cast<float>(value.y()),
        static_cast<float>(value.z())};
}

PackedAnalyticLight pack_analytic_light(const AnalyticLightDesc& light) {
    return PackedAnalyticLight {
        .type = static_cast<PackedAnalyticLightType>(light.type),
        .position = pack_light_vector(light.position),
        .basis_x = pack_light_vector(light.local_to_world_linear.col(0)),
        .basis_y = pack_light_vector(light.local_to_world_linear.col(1)),
        .basis_z = pack_light_vector(light.local_to_world_linear.col(2)),
        .radiance = pack_light_vector(light.radiance),
        .radius = static_cast<float>(light.radius),
        .width = static_cast<float>(light.width),
        .height = static_cast<float>(light.height),
        .length = static_cast<float>(light.length),
        .world_area = static_cast<float>(light.world_area),
        .cos_theta_max = static_cast<float>(light.cos_theta_max),
        .selection_pdf = static_cast<float>(light.selection_pdf),
        .cdf = static_cast<float>(light.cdf),
        .delta = light.delta ? 1 : 0,
        .treat_as_point = light.treat_as_point ? 1 : 0,
        .treat_as_line = light.treat_as_line ? 1 : 0,
    };
}

PackedSphere pack_sphere(const SpherePrimitive& sphere) {
    return PackedSphere {
        .center = sphere.center.cast<float>(),
        .radius = static_cast<float>(sphere.radius),
        .material_index = sphere.material_index,
    };
}

PackedQuad pack_quad(const QuadPrimitive& quad) {
    return PackedQuad {
        .origin = quad.origin.cast<float>(),
        .edge_u = quad.edge_u.cast<float>(),
        .edge_v = quad.edge_v.cast<float>(),
        .material_index = quad.material_index,
    };
}

PackedTriangle pack_triangle(const TrianglePrimitive& triangle) {
    return PackedTriangle {
        .p0 = triangle.p0.cast<float>(),
        .p1 = triangle.p1.cast<float>(),
        .p2 = triangle.p2.cast<float>(),
        .material_index = triangle.material_index,
    };
}

PackedMedium pack_medium(const HomogeneousMediumPrimitive& medium) {
    return PackedMedium {
        .local_center_or_min = medium.local_center_or_min.cast<float>(),
        .radius = static_cast<float>(medium.radius),
        .local_max = medium.local_max.cast<float>(),
        .density = static_cast<float>(medium.density),
        .rotation_row0 = medium.world_to_local_rotation.row(0).transpose().cast<float>(),
        .material_index = medium.material_index,
        .rotation_row1 = medium.world_to_local_rotation.row(1).transpose().cast<float>(),
        .boundary_type = medium.boundary_type,
        .rotation_row2 = medium.world_to_local_rotation.row(2).transpose().cast<float>(),
        .translation = medium.translation.cast<float>(),
    };
}

MaterialSample pack_material(const MaterialDesc& material,
    std::vector<OpenPbrCompiledMaterial>& openpbr_materials) {
    MaterialSample sample {};
    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, LambertianMaterial>) {
                sample.albedo_texture = value.albedo_texture;
                sample.type = 0;
            } else if constexpr (std::is_same_v<T, MetalMaterial>) {
                sample.albedo_texture = value.albedo_texture;
                sample.fuzz = static_cast<float>(value.fuzz);
                sample.type = 1;
            } else if constexpr (std::is_same_v<T, DielectricMaterial>) {
                sample.ior = static_cast<float>(value.ior);
                sample.type = 2;
            } else if constexpr (std::is_same_v<T, DiffuseLightMaterial>) {
                sample.emission_texture = value.emission_texture;
                sample.type = 3;
            } else if constexpr (std::is_same_v<T, IsotropicVolumeMaterial>) {
                sample.albedo_texture = value.albedo_texture;
                sample.type = 4;
            } else if constexpr (std::is_same_v<T, OpenPbrMaterialDesc>) {
                sample.openpbr_index = static_cast<int>(openpbr_materials.size());
                openpbr_materials.push_back(value.compiled);
                sample.type = 5;
            }
        },
        material);
    return sample;
}

cv::Mat load_texture_image_rgb32f(const std::string& path) {
    cv::Mat bgr = cv::imread(path, cv::IMREAD_COLOR);
    if (bgr.empty()) {
        const std::array<std::string, 2> fallbacks {std::string("../") + path,
            std::string("../../") + path};
        for (const std::string& fallback : fallbacks) {
            bgr = cv::imread(fallback, cv::IMREAD_COLOR);
            if (!bgr.empty()) {
                break;
            }
        }
    }
    if (bgr.empty()) {
        return {};
    }

    cv::Mat rgb_u8;
    cv::cvtColor(bgr, rgb_u8, cv::COLOR_BGR2RGB);
    cv::Mat rgb_f32;
    rgb_u8.convertTo(rgb_f32, CV_32FC3, 1.0 / 255.0);
    return rgb_f32;
}

PackedTexture pack_texture(const TextureDesc& texture, std::vector<Eigen::Vector3f>& image_texels) {
    PackedTexture packed {};
    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, ConstantColorTextureDesc>) {
                packed.type = 0;
                packed.color = value.color.template cast<float>();
            } else if constexpr (std::is_same_v<T, CheckerTextureDesc>) {
                packed.type = 1;
                packed.scale = static_cast<float>(value.scale);
                packed.even_texture = value.even_texture;
                packed.odd_texture = value.odd_texture;
            } else if constexpr (std::is_same_v<T, ImageTextureDesc>) {
                packed.type = 2;
                packed.image_offset = static_cast<int>(image_texels.size());
                const cv::Mat image = load_texture_image_rgb32f(value.path);
                if (image.empty()) {
                    return;
                }
                packed.image_width = image.cols;
                packed.image_height = image.rows;
                image_texels.reserve(
                    image_texels.size() + static_cast<std::size_t>(image.cols * image.rows));
                for (int y = 0; y < image.rows; ++y) {
                    for (int x = 0; x < image.cols; ++x) {
                        const cv::Vec3f pixel = image.at<cv::Vec3f>(y, x);
                        image_texels.emplace_back(pixel[0], pixel[1], pixel[2]);
                    }
                }
            } else if constexpr (std::is_same_v<T, NoiseTextureDesc>) {
                packed.type = 3;
                packed.scale = static_cast<float>(value.scale);
            }
        },
        texture);
    return packed;
}

float luminance(const Eigen::Vector3f& value) {
    return std::max(0.0f, 0.2126f * value.x() + 0.7152f * value.y() + 0.0722f * value.z());
}

Eigen::Vector3f average_texture(const GpuPreparedScene& scene, int texture_index, int depth = 0) {
    if (texture_index < 0 || texture_index >= static_cast<int>(scene.textures.size())
        || depth >= 8) {
        return Eigen::Vector3f::Zero();
    }
    const PackedTexture& texture = scene.textures[static_cast<std::size_t>(texture_index)];
    if (texture.type == 0) {
        return texture.color.cwiseMax(0.0f);
    }
    if (texture.type == 1) {
        return 0.5f
               * (average_texture(scene, texture.even_texture, depth + 1)
                   + average_texture(scene, texture.odd_texture, depth + 1));
    }
    if (texture.type == 2 && texture.image_width > 0 && texture.image_height > 0) {
        const int count = texture.image_width * texture.image_height;
        if (texture.image_offset < 0
            || texture.image_offset + count > static_cast<int>(scene.image_texels.size())) {
            return Eigen::Vector3f::Zero();
        }
        Eigen::Vector3f sum = Eigen::Vector3f::Zero();
        for (int i = 0; i < count; ++i) {
            sum += scene.image_texels[static_cast<std::size_t>(texture.image_offset + i)];
        }
        return (sum / static_cast<float>(count)).cwiseMax(0.0f);
    }
    if (texture.type == 3) {
        return Eigen::Vector3f::Constant(0.5f);
    }
    return Eigen::Vector3f::Zero();
}

float material_emission_luminance(const GpuPreparedScene& scene, int material_index) {
    if (material_index < 0 || material_index >= static_cast<int>(scene.materials.size())) {
        return 0.0f;
    }
    const MaterialSample& material = scene.materials[static_cast<std::size_t>(material_index)];
    if (material.type == 3) {
        return luminance(average_texture(scene, material.emission_texture));
    }
    if (material.type != 5 || material.openpbr_index < 0
        || material.openpbr_index >= static_cast<int>(scene.openpbr_materials.size())) {
        return 0.0f;
    }

    OpenPbrCoreMaterial parameters =
        scene.openpbr_materials[static_cast<std::size_t>(material.openpbr_index)].parameters;
    const OpenPbrColorTextureBinding& binding =
        scene.openpbr_materials[static_cast<std::size_t>(material.openpbr_index)]
            .color_textures.emission_color;
    if (binding.texture_index >= 0) {
        const Eigen::Vector3f sampled = average_texture(scene, binding.texture_index);
        openpbr_apply_color_input(parameters, OpenPbrColorInput::emission_color,
            {sampled.x(), sampled.y(), sampled.z()}, binding.source_color_space);
    }
    return openpbr_luminance(emission_openpbr_core(parameters));
}

void append_light(std::vector<PackedLight>& lights, PackedLightType type, int primitive_index,
    float weight) {
    if (!std::isfinite(weight) || weight <= 1e-12f) {
        return;
    }
    lights.push_back(PackedLight {
        .type = type,
        .primitive_index = primitive_index,
        .selection_pdf = weight,
    });
}

void build_light_distribution(GpuPreparedScene& prepared) {
    constexpr float kPi = 3.14159265358979323846f;
    for (std::size_t i = 0; i < prepared.spheres.size(); ++i) {
        const PackedSphere& sphere = prepared.spheres[i];
        const float area = 4.0f * kPi * sphere.radius * sphere.radius;
        append_light(prepared.lights, PackedLightType::sphere, static_cast<int>(i),
            area * material_emission_luminance(prepared, sphere.material_index));
    }
    for (std::size_t i = 0; i < prepared.quads.size(); ++i) {
        const PackedQuad& quad = prepared.quads[i];
        const float area = quad.edge_u.cross(quad.edge_v).norm();
        append_light(prepared.lights, PackedLightType::quad, static_cast<int>(i),
            area * material_emission_luminance(prepared, quad.material_index));
    }
    for (std::size_t i = 0; i < prepared.triangles.size(); ++i) {
        const PackedTriangle& triangle = prepared.triangles[i];
        const float area =
            0.5f * (triangle.p1 - triangle.p0).cross(triangle.p2 - triangle.p0).norm();
        append_light(prepared.lights, PackedLightType::triangle, static_cast<int>(i),
            area * material_emission_luminance(prepared, triangle.material_index));
    }
    append_light(prepared.lights, PackedLightType::environment, -1,
        4.0f * kPi * luminance(prepared.background));

    float total_weight = 0.0f;
    for (const PackedLight& light : prepared.lights) {
        total_weight += light.selection_pdf;
    }
    float cdf = 0.0f;
    for (PackedLight& light : prepared.lights) {
        light.selection_pdf /= total_weight;
        cdf += light.selection_pdf;
        light.cdf = cdf;
    }
    if (!prepared.lights.empty()) {
        prepared.lights.back().cdf = 1.0f;
    }
}

} // namespace

GpuPreparedScene prepare_gpu_scene(const PackedScene& scene) {
    GpuPreparedScene prepared {};
    prepared.background = scene.background.cast<float>();

    prepared.spheres.reserve(scene.spheres.size());
    for (const SpherePrimitive& sphere : scene.spheres) {
        prepared.spheres.push_back(pack_sphere(sphere));
    }

    prepared.quads.reserve(scene.quads.size());
    for (const QuadPrimitive& quad : scene.quads) {
        prepared.quads.push_back(pack_quad(quad));
    }

    prepared.triangles.reserve(scene.triangles.size());
    for (const TrianglePrimitive& triangle : scene.triangles) {
        prepared.triangles.push_back(pack_triangle(triangle));
    }

    prepared.media.reserve(scene.media.size());
    for (const HomogeneousMediumPrimitive& medium : scene.media) {
        prepared.media.push_back(pack_medium(medium));
    }

    prepared.materials.reserve(scene.materials.size());
    for (const MaterialDesc& material : scene.materials) {
        prepared.materials.push_back(pack_material(material, prepared.openpbr_materials));
    }

    prepared.textures.reserve(scene.textures.size());
    for (const TextureDesc& texture : scene.textures) {
        prepared.textures.push_back(pack_texture(texture, prepared.image_texels));
    }

    prepared.analytic_lights.reserve(scene.analytic_lights.size());
    for (const AnalyticLightDesc& light : scene.analytic_lights) {
        prepared.analytic_lights.push_back(pack_analytic_light(light));
    }

    build_light_distribution(prepared);

    return prepared;
}

} // namespace rt
