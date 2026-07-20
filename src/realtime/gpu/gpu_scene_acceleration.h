#pragma once

#include "realtime/gpu/launch_params.h"
#include "realtime/gpu/packed_scene_preparation.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace rt {

enum class AccelerationUpdateKind {
    reuse,
    update,
    refit,
    rebuild,
};

std::string_view acceleration_update_kind_name(AccelerationUpdateKind kind);

struct AccelerationUpdateStats {
    AccelerationUpdateKind kind = AccelerationUpdateKind::rebuild;
    double elapsed_ms = 0.0;
    int node_count = 0;
    int primitive_reference_count = 0;
    int prototype_count = 0;
    int instance_count = 0;
    int instanced_primitive_count = 0;
    std::uint64_t generation = 0;
};

class GpuSceneAcceleration {
public:
    AccelerationUpdateStats update(const GpuPreparedScene& scene);
    void reset();

    const std::vector<PackedBvhNode>& nodes() const;
    const std::vector<PackedPrimitiveRef>& references() const;
    const AccelerationUpdateStats& last_update() const;

private:
    void rebuild(const GpuPreparedScene& scene);
    void refit(const GpuPreparedScene& scene);

    std::vector<PackedBvhNode> nodes_;
    std::vector<PackedPrimitiveRef> references_;
    AccelerationUpdateStats last_update_ {};
    std::uint64_t geometry_signature_ = 0;
    std::uint64_t scene_signature_ = 0;
    std::uint64_t generation_ = 0;
    int sphere_count_ = -1;
    int quad_count_ = -1;
    int triangle_count_ = -1;
};

} // namespace rt
