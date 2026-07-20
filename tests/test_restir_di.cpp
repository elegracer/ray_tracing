#include "common/restir_di.h"

#include "test_support.h"

#include <cmath>
#include <iostream>

namespace {

rt::RestirCandidate candidate(int light_index, float u0 = 0.25f, float u1 = 0.75f) {
    return rt::RestirCandidate {
        .light_index = light_index,
        .sample_u0 = u0,
        .sample_u1 = u1,
    };
}

rt::RestirSurface surface(float position_x = 0.0f, int material_type = 5,
    int primitive_index = 7) {
    return rt::RestirSurface {
        .position = {.x = position_x, .y = 0.0f, .z = 2.0f},
        .normal = {.x = 0.0f, .y = 0.0f, .z = 1.0f},
        .material_type = material_type,
        .primitive_type = 2,
        .primitive_index = primitive_index,
    };
}

} // namespace

int main() {
    rt::RestirReservoir reservoir {};
    rt::restir_update(reservoir, candidate(3), 1.0f, 1.0f, 1, 0.0f);
    rt::restir_update(reservoir, candidate(9), 3.0f, 3.0f, 1, 0.99f);
    rt::restir_update(reservoir, candidate(11), 0.0f, 0.0f, 1, 0.5f);
    rt::restir_finalize(reservoir);

    expect_true(reservoir.valid != 0, "positive candidates produce a valid reservoir");
    expect_true(reservoir.selected.light_index == 3,
        "weighted replacement keeps the deterministic first candidate");
    expect_true(reservoir.candidate_count == 3, "zero-target null events still count in M");
    expect_near(reservoir.weight_sum, 4.0, 1e-7, "reservoir weight sum");
    expect_near(reservoir.estimator_weight, 4.0 / 3.0, 1e-7,
        "reservoir estimator normalization");

    rt::RestirReservoir previous {};
    previous.selected = candidate(21, 0.1f, 0.2f);
    previous.surface = surface();
    previous.weight_sum = 16.0f;
    previous.selected_target = 4.0f;
    previous.estimator_weight = 0.5f;
    previous.candidate_count = 8;
    previous.age = 3;
    previous.valid = 1;

    rt::RestirReservoir merged {};
    rt::restir_update(merged, candidate(4), 1.0f, 2.0f, 1, 0.0f);
    rt::restir_merge_temporal(merged, previous, 2.0f, 4, 0.0f);
    merged.surface = surface();
    rt::restir_finalize(merged);
    expect_true(merged.selected.light_index == 21, "temporal candidate can replace current sample");
    expect_true(merged.candidate_count == 5, "temporal M is clamped before merge");
    expect_true(merged.temporal_candidate_count == 4, "temporal multiplicity is recorded");
    expect_true(merged.age == 4, "temporal age advances once");
    expect_near(merged.weight_sum, 6.0, 1e-7, "temporal merge reweights at current target");
    expect_near(merged.estimator_weight, 0.6, 1e-7, "merged estimator normalization");

    expect_true(rt::restir_temporal_surface_valid(surface(), previous, 0.01f, 0.95f, 20),
        "matching surface accepts temporal reuse");
    expect_true(!rt::restir_temporal_surface_valid(surface(0.02f), previous, 0.01f, 0.95f, 20),
        "position discontinuity rejects temporal reuse");
    rt::RestirSurface flipped = surface();
    flipped.normal.z = -1.0f;
    expect_true(!rt::restir_temporal_surface_valid(flipped, previous, 0.01f, 0.95f, 20),
        "normal discontinuity rejects temporal reuse");
    expect_true(!rt::restir_temporal_surface_valid(surface(0.0f, 0), previous, 0.01f, 0.95f, 20),
        "material change rejects temporal reuse");
    expect_true(!rt::restir_temporal_surface_valid(surface(0.0f, 5, 8), previous, 0.01f, 0.95f, 20),
        "primitive identity change rejects temporal reuse");
    previous.age = 20;
    expect_true(!rt::restir_temporal_surface_valid(surface(), previous, 0.01f, 0.95f, 20),
        "expired reservoir rejects temporal reuse");

    std::cout << "restir reservoir M=" << merged.candidate_count
              << " W=" << merged.estimator_weight
              << " temporal_M=" << merged.temporal_candidate_count << '\n';
    return 0;
}
