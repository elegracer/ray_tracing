#include "realtime/gpu/gpu_scene_acceleration.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace rt {
namespace {

constexpr int kLeafSize = 4;
constexpr float kBoundsPadding = 1e-5f;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

struct Bounds {
    Eigen::Vector3f min = Eigen::Vector3f::Constant(std::numeric_limits<float>::infinity());
    Eigen::Vector3f max = Eigen::Vector3f::Constant(-std::numeric_limits<float>::infinity());
};

struct BuildReference {
    PackedPrimitiveRef reference;
    Bounds bounds;
};

void extend(Bounds& bounds, const Eigen::Vector3f& point) {
    bounds.min = bounds.min.cwiseMin(point);
    bounds.max = bounds.max.cwiseMax(point);
}

void extend(Bounds& bounds, const Bounds& other) {
    extend(bounds, other.min);
    extend(bounds, other.max);
}

Bounds padded_bounds(Bounds bounds) {
    for (int axis = 0; axis < 3; ++axis) {
        if (bounds.max[axis] - bounds.min[axis] < kBoundsPadding) {
            bounds.min[axis] -= kBoundsPadding;
            bounds.max[axis] += kBoundsPadding;
        }
    }
    return bounds;
}

Bounds sphere_bounds(const PackedSphere& sphere) {
    const Eigen::Vector3f radius = Eigen::Vector3f::Constant(std::abs(sphere.radius));
    return Bounds {.min = sphere.center - radius, .max = sphere.center + radius};
}

Bounds quad_bounds(const PackedQuad& quad) {
    Bounds bounds;
    extend(bounds, quad.origin);
    extend(bounds, quad.origin + quad.edge_u);
    extend(bounds, quad.origin + quad.edge_v);
    extend(bounds, quad.origin + quad.edge_u + quad.edge_v);
    return padded_bounds(bounds);
}

Bounds triangle_bounds(const PackedTriangle& triangle) {
    Bounds bounds;
    extend(bounds, triangle.p0);
    extend(bounds, triangle.p1);
    extend(bounds, triangle.p2);
    return padded_bounds(bounds);
}

Bounds reference_bounds(const GpuPreparedScene& scene, const PackedPrimitiveRef& reference) {
    switch (static_cast<PackedPrimitiveType>(reference.primitive_type)) {
        case PackedPrimitiveType::sphere:
            return sphere_bounds(
                scene.spheres.at(static_cast<std::size_t>(reference.primitive_index)));
        case PackedPrimitiveType::quad:
            return quad_bounds(scene.quads.at(static_cast<std::size_t>(reference.primitive_index)));
        case PackedPrimitiveType::triangle:
            return triangle_bounds(
                scene.triangles.at(static_cast<std::size_t>(reference.primitive_index)));
    }
    throw std::logic_error("unknown packed acceleration primitive type");
}

Eigen::Vector3f centroid(const Bounds& bounds) {
    return (bounds.min + bounds.max) * 0.5f;
}

std::uint64_t hash_bytes(std::uint64_t hash, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kFnvPrime;
    }
    return hash;
}

template<typename T>
std::uint64_t hash_value(std::uint64_t hash, const T& value) {
    return hash_bytes(hash, &value, sizeof(T));
}

template<typename T>
std::uint64_t hash_vector(std::uint64_t hash, const std::vector<T>& values) {
    hash = hash_value(hash, values.size());
    if (!values.empty()) {
        hash = hash_bytes(hash, values.data(), values.size() * sizeof(T));
    }
    return hash;
}

std::uint64_t geometry_signature(const GpuPreparedScene& scene) {
    std::uint64_t hash = kFnvOffset;
    hash = hash_vector(hash, scene.spheres);
    hash = hash_vector(hash, scene.quads);
    hash = hash_vector(hash, scene.triangles);
    return hash;
}

std::uint64_t scene_signature(const GpuPreparedScene& scene) {
    std::uint64_t hash = geometry_signature(scene);
    hash = hash_value(hash, scene.background);
    hash = hash_vector(hash, scene.media);
    hash = hash_vector(hash, scene.textures);
    hash = hash_vector(hash, scene.image_texels);
    hash = hash_vector(hash, scene.materials);
    hash = hash_vector(hash, scene.openpbr_materials);
    hash = hash_vector(hash, scene.lights);
    hash = hash_vector(hash, scene.analytic_lights);
    return hash;
}

int largest_axis(const Bounds& bounds) {
    const Eigen::Vector3f extent = bounds.max - bounds.min;
    if (extent.y() > extent.x() && extent.y() >= extent.z()) {
        return 1;
    }
    return extent.z() > extent.x() ? 2 : 0;
}

int build_node(std::vector<PackedBvhNode>& nodes, std::vector<BuildReference>& references,
    int begin, int end) {
    const int node_index = static_cast<int>(nodes.size());
    nodes.emplace_back();

    Bounds node_bounds;
    Bounds centroid_bounds;
    for (int i = begin; i < end; ++i) {
        extend(node_bounds, references[static_cast<std::size_t>(i)].bounds);
        extend(centroid_bounds, centroid(references[static_cast<std::size_t>(i)].bounds));
    }

    const int count = end - begin;
    PackedBvhNode& node = nodes[static_cast<std::size_t>(node_index)];
    node.bounds_min = node_bounds.min;
    node.bounds_max = node_bounds.max;
    if (count <= kLeafSize) {
        node.first_reference = begin;
        node.reference_count = count;
        return node_index;
    }

    const int axis = largest_axis(centroid_bounds);
    const int middle = begin + count / 2;
    std::nth_element(references.begin() + begin, references.begin() + middle,
        references.begin() + end, [&](const BuildReference& lhs, const BuildReference& rhs) {
            return centroid(lhs.bounds)[axis] < centroid(rhs.bounds)[axis];
        });
    const int left = build_node(nodes, references, begin, middle);
    const int right = build_node(nodes, references, middle, end);
    PackedBvhNode& completed = nodes[static_cast<std::size_t>(node_index)];
    completed.left_child = left;
    completed.right_child = right;
    return node_index;
}

void append_reference(std::vector<BuildReference>& out, PackedPrimitiveType type, int index,
    int prototype_id, int instance_id, const Bounds& bounds) {
    out.push_back(BuildReference {
        .reference =
            PackedPrimitiveRef {
                .primitive_type = static_cast<int>(type),
                .primitive_index = index,
                .prototype_id = prototype_id,
                .instance_id = instance_id,
            },
        .bounds = bounds,
    });
}

AccelerationUpdateStats instance_stats(const std::vector<PackedPrimitiveRef>& references) {
    std::set<int> prototypes;
    std::set<int> instances;
    std::set<std::pair<int, int>> prototype_instances;
    int fallback_identity = -1;
    for (const PackedPrimitiveRef& reference : references) {
        const int prototype =
            reference.prototype_id >= 0 ? reference.prototype_id : fallback_identity--;
        const int instance =
            reference.instance_id >= 0 ? reference.instance_id : fallback_identity--;
        prototypes.insert(prototype);
        instances.insert(instance);
        prototype_instances.emplace(prototype, instance);
    }

    std::set<int> repeated_prototypes;
    for (auto it = prototype_instances.begin(); it != prototype_instances.end();) {
        const int prototype = it->first;
        int count = 0;
        while (it != prototype_instances.end() && it->first == prototype) {
            ++count;
            ++it;
        }
        if (count > 1) {
            repeated_prototypes.insert(prototype);
        }
    }
    int instanced_primitive_count = 0;
    for (const PackedPrimitiveRef& reference : references) {
        if (reference.prototype_id >= 0 && repeated_prototypes.contains(reference.prototype_id)) {
            ++instanced_primitive_count;
        }
    }
    return AccelerationUpdateStats {
        .prototype_count = static_cast<int>(prototypes.size()),
        .instance_count = static_cast<int>(instances.size()),
        .instanced_primitive_count = instanced_primitive_count,
    };
}

} // namespace

std::string_view acceleration_update_kind_name(AccelerationUpdateKind kind) {
    switch (kind) {
        case AccelerationUpdateKind::reuse: return "reuse";
        case AccelerationUpdateKind::update: return "update";
        case AccelerationUpdateKind::refit: return "refit";
        case AccelerationUpdateKind::rebuild: return "rebuild";
    }
    return "unknown";
}

AccelerationUpdateStats GpuSceneAcceleration::update(const GpuPreparedScene& scene) {
    const auto begin = std::chrono::steady_clock::now();
    const std::uint64_t next_geometry_signature = geometry_signature(scene);
    const std::uint64_t next_scene_signature = scene_signature(scene);
    const bool topology_changed = sphere_count_ != static_cast<int>(scene.spheres.size())
                                  || quad_count_ != static_cast<int>(scene.quads.size())
                                  || triangle_count_ != static_cast<int>(scene.triangles.size());

    AccelerationUpdateKind kind = AccelerationUpdateKind::reuse;
    if (topology_changed || nodes_.empty()) {
        rebuild(scene);
        kind = AccelerationUpdateKind::rebuild;
        ++generation_;
    } else if (geometry_signature_ != next_geometry_signature) {
        refit(scene);
        kind = AccelerationUpdateKind::refit;
        ++generation_;
    } else if (scene_signature_ != next_scene_signature) {
        kind = AccelerationUpdateKind::update;
        ++generation_;
    }

    sphere_count_ = static_cast<int>(scene.spheres.size());
    quad_count_ = static_cast<int>(scene.quads.size());
    triangle_count_ = static_cast<int>(scene.triangles.size());
    geometry_signature_ = next_geometry_signature;
    scene_signature_ = next_scene_signature;

    const auto end = std::chrono::steady_clock::now();
    const AccelerationUpdateStats instances = instance_stats(references_);
    last_update_ = AccelerationUpdateStats {
        .kind = kind,
        .elapsed_ms = std::chrono::duration<double, std::milli>(end - begin).count(),
        .node_count = static_cast<int>(nodes_.size()),
        .primitive_reference_count = static_cast<int>(references_.size()),
        .prototype_count = instances.prototype_count,
        .instance_count = instances.instance_count,
        .instanced_primitive_count = instances.instanced_primitive_count,
        .generation = generation_,
    };
    return last_update_;
}

void GpuSceneAcceleration::reset() {
    nodes_.clear();
    references_.clear();
    last_update_ = {};
    geometry_signature_ = 0;
    scene_signature_ = 0;
    generation_ = 0;
    sphere_count_ = -1;
    quad_count_ = -1;
    triangle_count_ = -1;
}

const std::vector<PackedBvhNode>& GpuSceneAcceleration::nodes() const {
    return nodes_;
}

const std::vector<PackedPrimitiveRef>& GpuSceneAcceleration::references() const {
    return references_;
}

const AccelerationUpdateStats& GpuSceneAcceleration::last_update() const {
    return last_update_;
}

void GpuSceneAcceleration::rebuild(const GpuPreparedScene& scene) {
    std::vector<BuildReference> build_references;
    build_references.reserve(scene.spheres.size() + scene.quads.size() + scene.triangles.size());
    for (std::size_t i = 0; i < scene.spheres.size(); ++i) {
        const PackedSphere& sphere = scene.spheres[i];
        append_reference(build_references, PackedPrimitiveType::sphere, static_cast<int>(i),
            sphere.acceleration_prototype_id, sphere.acceleration_instance_id,
            sphere_bounds(sphere));
    }
    for (std::size_t i = 0; i < scene.quads.size(); ++i) {
        const PackedQuad& quad = scene.quads[i];
        append_reference(build_references, PackedPrimitiveType::quad, static_cast<int>(i),
            quad.acceleration_prototype_id, quad.acceleration_instance_id, quad_bounds(quad));
    }
    for (std::size_t i = 0; i < scene.triangles.size(); ++i) {
        const PackedTriangle& triangle = scene.triangles[i];
        append_reference(build_references, PackedPrimitiveType::triangle, static_cast<int>(i),
            triangle.acceleration_prototype_id, triangle.acceleration_instance_id,
            triangle_bounds(triangle));
    }
    if (build_references.empty()) {
        throw std::invalid_argument("GPU acceleration structure requires at least one surface");
    }

    nodes_.clear();
    nodes_.reserve(build_references.size() * 2U);
    build_node(nodes_, build_references, 0, static_cast<int>(build_references.size()));
    references_.clear();
    references_.reserve(build_references.size());
    for (const BuildReference& reference : build_references) {
        references_.push_back(reference.reference);
    }
}

void GpuSceneAcceleration::refit(const GpuPreparedScene& scene) {
    for (std::size_t node_index = nodes_.size(); node_index-- > 0;) {
        PackedBvhNode& node = nodes_[node_index];
        Bounds bounds;
        if (node.reference_count > 0) {
            for (int i = 0; i < node.reference_count; ++i) {
                const std::size_t reference_index =
                    static_cast<std::size_t>(node.first_reference + i);
                extend(bounds, reference_bounds(scene, references_.at(reference_index)));
            }
        } else {
            const PackedBvhNode& left = nodes_.at(static_cast<std::size_t>(node.left_child));
            const PackedBvhNode& right = nodes_.at(static_cast<std::size_t>(node.right_child));
            extend(bounds, Bounds {.min = left.bounds_min, .max = left.bounds_max});
            extend(bounds, Bounds {.min = right.bounds_min, .max = right.bounds_max});
        }
        node.bounds_min = bounds.min;
        node.bounds_max = bounds.max;
    }
}

} // namespace rt
