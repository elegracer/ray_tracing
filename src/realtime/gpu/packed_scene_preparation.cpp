#include "realtime/gpu/packed_scene_preparation.h"

#include <opencv2/opencv.hpp>

#include <array>
#include <type_traits>

namespace rt {
namespace {

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

MaterialSample pack_material(const MaterialDesc& material) {
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
            }
        },
        material);
    return sample;
}

cv::Mat load_texture_image_rgb32f(const std::string& path) {
    cv::Mat bgr = cv::imread(path, cv::IMREAD_COLOR);
    if (bgr.empty()) {
        const std::array<std::string, 2> fallbacks {std::string("../") + path, std::string("../../") + path};
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
                image_texels.reserve(image_texels.size() + static_cast<std::size_t>(image.cols * image.rows));
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

}  // namespace

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
        prepared.materials.push_back(pack_material(material));
    }

    prepared.textures.reserve(scene.textures.size());
    for (const TextureDesc& texture : scene.textures) {
        prepared.textures.push_back(pack_texture(texture, prepared.image_texels));
    }

    return prepared;
}

}  // namespace rt
