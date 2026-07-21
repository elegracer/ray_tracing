#pragma once

#include "common/light_sampling.h"

#include <cmath>

namespace rt {

#if defined(__CUDACC__)
#define RT_RESTIR_HD     __host__ __device__
#define RT_RESTIR_INLINE __forceinline__
#else
#define RT_RESTIR_HD
#define RT_RESTIR_INLINE inline
#endif

enum class RestirBiasCorrectionMode : int {
    off = 0,
    basic = 1,
};

struct RestirSurface {
    PackedLightVector3 position;
    PackedLightVector3 normal;
    int material_type = -1;
    int primitive_type = -1;
    int primitive_index = -1;
};

struct RestirCandidate {
    int light_index = -1;
    float sample_u0 = 0.0f;
    float sample_u1 = 0.0f;
};

struct RestirReservoir {
    RestirCandidate selected;
    RestirSurface surface;
    float weight_sum = 0.0f;
    float selected_target = 0.0f;
    float estimator_weight = 0.0f;
    int candidate_count = 0;
    int temporal_candidate_count = 0;
    int spatial_candidate_count = 0;
    int bias_correction_source_count = 0;
    int age = 0;
    int valid = 0;
};

struct RestirMergeResult {
    int multiplicity = 0;
    int selected = 0;
};

RT_RESTIR_HD RT_RESTIR_INLINE float restir_clamp_unit(float value) {
    return value < 0.0f ? 0.0f : (value < 1.0f ? value : 0.99999994f);
}

RT_RESTIR_HD RT_RESTIR_INLINE bool restir_finite_positive(float value) {
#if defined(__CUDA_ARCH__)
    return value > 0.0f && isfinite(value);
#else
    return value > 0.0f && std::isfinite(value);
#endif
}

RT_RESTIR_HD RT_RESTIR_INLINE bool restir_update(RestirReservoir& reservoir,
    const RestirCandidate& candidate, float target_density, float candidate_weight,
    int candidate_multiplicity, float selection_sample) {
    if (candidate.light_index < 0 || candidate_multiplicity <= 0) {
        return false;
    }
    reservoir.candidate_count += candidate_multiplicity;
    if (!restir_finite_positive(target_density) || !restir_finite_positive(candidate_weight)) {
        return false;
    }

    const float next_weight_sum = reservoir.weight_sum + candidate_weight;
    if (!restir_finite_positive(next_weight_sum)) {
        return false;
    }
    const bool selected = restir_clamp_unit(selection_sample) * next_weight_sum < candidate_weight;
    if (selected) {
        reservoir.selected = candidate;
        reservoir.selected_target = target_density;
        reservoir.valid = 1;
    }
    reservoir.weight_sum = next_weight_sum;
    return selected;
}

RT_RESTIR_HD RT_RESTIR_INLINE void restir_finalize(RestirReservoir& reservoir) {
    reservoir.estimator_weight = 0.0f;
    if (reservoir.valid == 0 || reservoir.candidate_count <= 0
        || !restir_finite_positive(reservoir.selected_target)
        || !restir_finite_positive(reservoir.weight_sum)) {
        reservoir.valid = 0;
        return;
    }
    reservoir.estimator_weight =
        reservoir.weight_sum
        / (static_cast<float>(reservoir.candidate_count) * reservoir.selected_target);
    if (!restir_finite_positive(reservoir.estimator_weight)) {
        reservoir.valid = 0;
        reservoir.estimator_weight = 0.0f;
    }
}

RT_RESTIR_HD RT_RESTIR_INLINE void restir_finalize_basic_bias_correction(RestirReservoir& reservoir,
    float selected_source_target, float source_target_sum, int source_count) {
    reservoir.estimator_weight = 0.0f;
    reservoir.bias_correction_source_count = source_count > 0 ? source_count : 0;
    if (reservoir.valid == 0 || source_count <= 0
        || !restir_finite_positive(reservoir.selected_target)
        || !restir_finite_positive(reservoir.weight_sum)
        || !restir_finite_positive(selected_source_target)
        || !restir_finite_positive(source_target_sum)) {
        reservoir.valid = 0;
        return;
    }
    reservoir.estimator_weight = reservoir.weight_sum * selected_source_target
                                 / (reservoir.selected_target * source_target_sum);
    if (!restir_finite_positive(reservoir.estimator_weight)) {
        reservoir.valid = 0;
        reservoir.estimator_weight = 0.0f;
    }
}

RT_RESTIR_HD RT_RESTIR_INLINE float restir_dot(const PackedLightVector3& lhs,
    const PackedLightVector3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

RT_RESTIR_HD RT_RESTIR_INLINE float restir_distance_squared(const PackedLightVector3& lhs,
    const PackedLightVector3& rhs) {
    const float x = lhs.x - rhs.x;
    const float y = lhs.y - rhs.y;
    const float z = lhs.z - rhs.z;
    return x * x + y * y + z * z;
}

RT_RESTIR_HD RT_RESTIR_INLINE bool restir_temporal_surface_valid(const RestirSurface& current,
    const RestirReservoir& previous, float max_position_distance, float min_normal_dot,
    int max_history_age) {
    if (previous.valid == 0 || previous.selected.light_index < 0 || previous.age < 0
        || previous.age >= max_history_age
        || current.material_type != previous.surface.material_type
        || current.primitive_type != previous.surface.primitive_type
        || current.primitive_index != previous.surface.primitive_index) {
        return false;
    }
    const float distance_limit = max_position_distance > 0.0f ? max_position_distance : 0.0f;
    return restir_distance_squared(current.position, previous.surface.position)
               <= distance_limit * distance_limit
           && restir_dot(current.normal, previous.surface.normal) >= min_normal_dot;
}

RT_RESTIR_HD RT_RESTIR_INLINE RestirMergeResult restir_merge_temporal(RestirReservoir& current,
    const RestirReservoir& previous, float target_density_at_current, int max_candidate_count,
    float selection_sample) {
    if (previous.valid == 0 || previous.candidate_count <= 0
        || !restir_finite_positive(previous.estimator_weight)) {
        return {};
    }
    int multiplicity = previous.candidate_count;
    if (max_candidate_count > 0 && multiplicity > max_candidate_count) {
        multiplicity = max_candidate_count;
    }
    const float candidate_weight = restir_finite_positive(target_density_at_current)
                                       ? target_density_at_current * previous.estimator_weight
                                             * static_cast<float>(multiplicity)
                                       : 0.0f;
    const bool selected = restir_update(current, previous.selected, target_density_at_current,
        candidate_weight, multiplicity, selection_sample);
    current.temporal_candidate_count += multiplicity;
    current.age = previous.age + 1;
    return RestirMergeResult {.multiplicity = multiplicity, .selected = selected ? 1 : 0};
}

RT_RESTIR_HD RT_RESTIR_INLINE RestirMergeResult restir_merge_spatial(RestirReservoir& current,
    const RestirReservoir& previous, float target_density_at_current, int max_candidate_count,
    float selection_sample) {
    if (previous.valid == 0 || previous.candidate_count <= 0
        || !restir_finite_positive(previous.estimator_weight)) {
        return {};
    }
    int multiplicity = previous.candidate_count;
    if (max_candidate_count > 0 && multiplicity > max_candidate_count) {
        multiplicity = max_candidate_count;
    }
    const float candidate_weight = restir_finite_positive(target_density_at_current)
                                       ? target_density_at_current * previous.estimator_weight
                                             * static_cast<float>(multiplicity)
                                       : 0.0f;
    const bool selected = restir_update(current, previous.selected, target_density_at_current,
        candidate_weight, multiplicity, selection_sample);
    current.spatial_candidate_count += multiplicity;
    return RestirMergeResult {.multiplicity = multiplicity, .selected = selected ? 1 : 0};
}

#undef RT_RESTIR_HD
#undef RT_RESTIR_INLINE

} // namespace rt
