#include "common/light_sampling.h"

#include <cuda_runtime.h>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr int kDistributionSamples = 10000;

struct DeviceResults {
    float sphere_normalization = 0.0f;
    float cone_normalization = 0.0f;
    float area_round_trip = 0.0f;
    float power_partition = 0.0f;
    int cdf_counts[3] {0, 0, 0};
};

void check_cuda(cudaError_t error, const char* operation) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(error));
    }
}

void expect_near(double actual, double expected, double tolerance, const std::string& label) {
    if (std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(
            label + " actual=" + std::to_string(actual) + " expected=" + std::to_string(expected));
    }
}

__global__ void evaluate_light_sampling(DeviceResults* results) {
    const int index = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    if (index == 0) {
        constexpr float four_pi = 12.566370614359172953f;
        constexpr float cone_cosine = 0.8f;
        constexpr float cone_measure = 6.283185307179586477f * (1.0f - cone_cosine);
        results->sphere_normalization = rt::light_uniform_sphere_pdf() * four_pi;
        results->cone_normalization = rt::light_uniform_cone_pdf(cone_cosine) * cone_measure;
        const float area_pdf = rt::light_area_to_solid_angle_pdf(2.0f, 8.0f, 0.5f);
        results->area_round_trip = area_pdf * 2.0f * 0.5f / 8.0f;
        results->power_partition =
            rt::light_power_heuristic(0.2f, 0.7f) + rt::light_power_heuristic(0.7f, 0.2f);
    }
    if (index >= kDistributionSamples) {
        return;
    }

    constexpr rt::PackedAnalyticLight lights[3] {
        {.type = rt::PackedAnalyticLightType::sphere, .selection_pdf = 0.2f, .cdf = 0.2f},
        {.type = rt::PackedAnalyticLightType::rect, .selection_pdf = 0.3f, .cdf = 0.5f},
        {.type = rt::PackedAnalyticLightType::dome, .selection_pdf = 0.5f, .cdf = 1.0f},
    };
    const float sample = (static_cast<float>(index) + 0.5f) / kDistributionSamples;
    const int selected = rt::sample_packed_analytic_light(lights, 3, sample);
    if (selected >= 0) {
        atomicAdd(&results->cdf_counts[selected], 1);
    }
}

} // namespace

int main() {
    int device_count = 0;
    if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count == 0) {
        return 77;
    }

    DeviceResults* device_results = nullptr;
    check_cuda(cudaMalloc(reinterpret_cast<void**>(&device_results), sizeof(DeviceResults)),
        "cudaMalloc results");
    try {
        check_cuda(cudaMemset(device_results, 0, sizeof(DeviceResults)), "cudaMemset results");
        constexpr int threads = 256;
        constexpr int blocks = (kDistributionSamples + threads - 1) / threads;
        evaluate_light_sampling<<<blocks, threads>>>(device_results);
        check_cuda(cudaGetLastError(), "evaluate_light_sampling launch");

        DeviceResults actual;
        check_cuda(cudaMemcpy(&actual, device_results, sizeof(actual), cudaMemcpyDeviceToHost),
            "cudaMemcpy results");
        expect_near(actual.sphere_normalization, 1.0, 2e-6, "GPU uniform-sphere PDF normalization");
        expect_near(actual.cone_normalization, 1.0, 2e-6, "GPU uniform-cone PDF normalization");
        expect_near(actual.area_round_trip, 1.0, 2e-6, "GPU area/solid-angle Jacobian round trip");
        expect_near(actual.power_partition, 1.0, 2e-6, "GPU reciprocal power-heuristic partition");
        expect_near(actual.cdf_counts[0], 0.2 * kDistributionSamples, 0.0,
            "GPU analytic-light CDF first mass");
        expect_near(actual.cdf_counts[1], 0.3 * kDistributionSamples, 0.0,
            "GPU analytic-light CDF second mass");
        expect_near(actual.cdf_counts[2], 0.5 * kDistributionSamples, 0.0,
            "GPU analytic-light CDF third mass");
        std::cout << "gpu_pdf sphere=" << actual.sphere_normalization
                  << " cone=" << actual.cone_normalization << " area=" << actual.area_round_trip
                  << " cdf=" << actual.cdf_counts[0] << ',' << actual.cdf_counts[1] << ','
                  << actual.cdf_counts[2] << '\n';
    } catch (...) {
        cudaFree(device_results);
        throw;
    }
    check_cuda(cudaFree(device_results), "cudaFree results");
    return 0;
}
