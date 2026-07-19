#include "common/openpbr_core.h"

#include <cuda_runtime.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr int kCaseCount = 4;

struct CoreCase {
    rt::OpenPbrCoreMaterial material {};
    rt::OpenPbrFrame frame {};
    rt::OpenPbrVec3 wo {};
    rt::OpenPbrVec3 wi {};
    float u_lobe = 0.0f;
    float u1 = 0.0f;
    float u2 = 0.0f;
};

struct CoreOutput {
    rt::OpenPbrEvaluation evaluation {};
    rt::OpenPbrSample sample {};
    rt::OpenPbrVec3 emission {};
    rt::OpenPbrVec3 transmission_at_depth {};
};

void check_cuda(cudaError_t error, const char* operation) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(error));
    }
}

void expect_true(bool condition, const std::string& label) {
    if (!condition) {
        throw std::runtime_error("expect_true failed: " + label);
    }
}

void expect_near(double actual, double expected, double tolerance, const std::string& label) {
    if (std::abs(actual - expected) > tolerance) {
        throw std::runtime_error("expect_near failed: " + label
                                 + " actual=" + std::to_string(actual)
                                 + " expected=" + std::to_string(expected));
    }
}

void expect_relative(double actual, double expected, double tolerance, const std::string& label) {
    expect_near(actual, expected, tolerance * std::max(1.0, std::abs(expected)), label);
}

__global__ void evaluate_cases(const CoreCase* cases, CoreOutput* outputs) {
    const int index = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    if (index >= kCaseCount) {
        return;
    }
    const CoreCase& test_case = cases[index];
    outputs[index].evaluation =
        rt::evaluate_openpbr_core(test_case.material, test_case.frame, test_case.wo, test_case.wi);
    outputs[index].sample = rt::sample_openpbr_core(test_case.material, test_case.frame,
        test_case.wo, test_case.u_lobe, test_case.u1, test_case.u2);
    outputs[index].emission = rt::emission_openpbr_core(test_case.material);
    outputs[index].transmission_at_depth = rt::openpbr_transmission_at_distance(test_case.material,
        test_case.material.transmission_depth);
}

void expect_vec_near(const rt::OpenPbrVec3& actual, const rt::OpenPbrVec3& expected,
    double tolerance, const std::string& label) {
    expect_relative(actual.x, expected.x, tolerance, label + ".x");
    expect_relative(actual.y, expected.y, tolerance, label + ".y");
    expect_relative(actual.z, expected.z, tolerance, label + ".z");
}

std::array<CoreCase, kCaseCount> make_cases() {
    std::array<CoreCase, kCaseCount> cases {};
    for (CoreCase& test_case : cases) {
        test_case.frame = rt::make_openpbr_frame({0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f});
        test_case.wo = rt::openpbr_normalize({0.2f, -0.1f, 1.0f});
    }

    cases[0].material.specular_weight = 0.0f;
    cases[0].material.base_color = {0.7f, 0.2f, 0.1f};
    cases[0].wi = rt::openpbr_normalize({-0.3f, 0.15f, 1.0f});
    cases[0].u_lobe = 0.4f;
    cases[0].u1 = 0.25f;
    cases[0].u2 = 0.75f;

    cases[1].material.base_weight = 0.85f;
    cases[1].material.base_color = {0.9f, 0.65f, 0.2f};
    cases[1].material.base_metalness = 1.0f;
    cases[1].material.specular_roughness = 0.3f;
    cases[1].material.specular_roughness_anisotropy = 0.65f;
    cases[1].wi = rt::openpbr_normalize({0.45f, 0.1f, 1.0f});
    cases[1].u_lobe = 0.6f;
    cases[1].u1 = 0.35f;
    cases[1].u2 = 0.2f;

    cases[2].material.base_weight = 0.0f;
    cases[2].material.specular_ior = 1.45f;
    cases[2].material.specular_roughness = 0.24f;
    cases[2].material.transmission_weight = 1.0f;
    cases[2].material.transmission_color = {0.8f, 0.9f, 1.0f};
    cases[2].material.transmission_depth = 2.0f;
    cases[2].wi = rt::openpbr_normalize({-0.05f, 0.02f, -1.0f});
    cases[2].u_lobe = 0.8f;
    cases[2].u1 = 0.15f;
    cases[2].u2 = 0.45f;

    cases[3].material.base_weight = 0.0f;
    cases[3].material.transmission_weight = 1.0f;
    cases[3].material.geometry_thin_walled = 1;
    cases[3].material.geometry_opacity = 0.6f;
    cases[3].material.emission_luminance = 4.0f;
    cases[3].material.emission_color = {1.5f, 0.5f, 0.25f};
    cases[3].wi = rt::openpbr_negate(cases[3].wo);
    cases[3].u_lobe = 0.95f;
    cases[3].u1 = 0.5f;
    cases[3].u2 = 0.5f;
    return cases;
}

} // namespace

int main() {
    int device_count = 0;
    if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count == 0) {
        return 77;
    }

    const std::array<CoreCase, kCaseCount> cases = make_cases();
    std::array<CoreOutput, kCaseCount> expected {};
    for (int index = 0; index < kCaseCount; ++index) {
        expected[index].evaluation = rt::evaluate_openpbr_core(cases[index].material,
            cases[index].frame, cases[index].wo, cases[index].wi);
        expected[index].sample = rt::sample_openpbr_core(cases[index].material, cases[index].frame,
            cases[index].wo, cases[index].u_lobe, cases[index].u1, cases[index].u2);
        expected[index].emission = rt::emission_openpbr_core(cases[index].material);
        expected[index].transmission_at_depth = rt::openpbr_transmission_at_distance(
            cases[index].material, cases[index].material.transmission_depth);
    }

    CoreCase* device_cases = nullptr;
    CoreOutput* device_outputs = nullptr;
    check_cuda(cudaMalloc(reinterpret_cast<void**>(&device_cases), sizeof(cases)),
        "cudaMalloc cases");
    try {
        check_cuda(cudaMalloc(reinterpret_cast<void**>(&device_outputs), sizeof(expected)),
            "cudaMalloc outputs");
        check_cuda(cudaMemcpy(device_cases, cases.data(), sizeof(cases), cudaMemcpyHostToDevice),
            "cudaMemcpy cases");
        evaluate_cases<<<1, kCaseCount>>>(device_cases, device_outputs);
        check_cuda(cudaGetLastError(), "evaluate_cases launch");

        std::array<CoreOutput, kCaseCount> actual {};
        check_cuda(
            cudaMemcpy(actual.data(), device_outputs, sizeof(actual), cudaMemcpyDeviceToHost),
            "cudaMemcpy outputs");
        for (int index = 0; index < kCaseCount; ++index) {
            const std::string prefix = "case " + std::to_string(index);
            expect_vec_near(actual[index].evaluation.value, expected[index].evaluation.value, 2e-5,
                prefix + " evaluation");
            expect_relative(actual[index].evaluation.pdf, expected[index].evaluation.pdf, 5e-5,
                prefix + " PDF");
            expect_true(actual[index].sample.valid == expected[index].sample.valid,
                prefix + " sample validity");
            expect_true(actual[index].sample.delta == expected[index].sample.delta,
                prefix + " sample measure");
            expect_true(actual[index].sample.event == expected[index].sample.event,
                prefix + " sample event");
            expect_vec_near(actual[index].sample.wi, expected[index].sample.wi, 2e-5,
                prefix + " sample direction");
            expect_vec_near(actual[index].sample.value, expected[index].sample.value, 2e-5,
                prefix + " sample value");
            expect_vec_near(actual[index].sample.weight, expected[index].sample.weight, 3e-5,
                prefix + " sample weight");
            expect_relative(actual[index].sample.pdf, expected[index].sample.pdf, 5e-5,
                prefix + " sample PDF");
            expect_relative(actual[index].sample.discrete_pdf, expected[index].sample.discrete_pdf,
                2e-5, prefix + " sample discrete PDF");
            expect_vec_near(actual[index].emission, expected[index].emission, 2e-5,
                prefix + " emission");
            expect_vec_near(actual[index].transmission_at_depth,
                expected[index].transmission_at_depth, 2e-5,
                prefix + " transmission at authored depth");
        }

        cudaFree(device_outputs);
        cudaFree(device_cases);
    } catch (...) {
        cudaFree(device_outputs);
        cudaFree(device_cases);
        throw;
    }
    return 0;
}
