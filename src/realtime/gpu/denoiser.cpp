#include "realtime/gpu/denoiser.h"

#include "realtime/gpu/cuda_event_timer.h"

#include <optix_stubs.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace rt {
namespace {

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(
            std::string("CUDA runtime failure at ") + expr + ": " + cudaGetErrorString(error));
    }
}

void throw_optix_error(OptixResult result, const char* expr) {
    if (result != OPTIX_SUCCESS) {
        throw std::runtime_error(
            std::string("OptiX denoiser failure at ") + expr + ": " + std::to_string(result));
    }
}

#define RT_CUDA_CHECK(expr)  throw_cuda_error((expr), #expr)
#define RT_OPTIX_CHECK(expr) throw_optix_error((expr), #expr)

void allocate_device(CUdeviceptr& ptr, std::size_t size) {
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&ptr), size));
}

void free_device(CUdeviceptr& ptr) {
    if (ptr != 0) {
        cudaFree(reinterpret_cast<void*>(ptr));
        ptr = 0;
    }
}

OptixImage2D make_image(CUdeviceptr data, unsigned int width, unsigned int height,
    unsigned int pixel_stride, OptixPixelFormat format) {
    return OptixImage2D {
        .data = data,
        .width = width,
        .height = height,
        .rowStrideInBytes = width * pixel_stride,
        .pixelStrideInBytes = pixel_stride,
        .format = format,
    };
}

CUdeviceptr device_ptr(const void* ptr) {
    return reinterpret_cast<CUdeviceptr>(ptr);
}

} // namespace

OptixDenoiserWrapper::~OptixDenoiserWrapper() {
    shutdown();
}

void OptixDenoiserWrapper::initialize(OptixDeviceContext context, cudaStream_t stream, int width,
    int height) {
    shutdown();
    if (context == nullptr || stream == nullptr || width <= 0 || height <= 0) {
        throw std::invalid_argument("OptiX denoiser requires a valid context, stream, and extent");
    }

    context_ = context;
    stream_ = stream;
    width_ = width;
    height_ = height;

    try {
        OptixDenoiserOptions options {};
        options.guideAlbedo = 1;
        options.guideNormal = 1;
        options.denoiseAlpha = OPTIX_DENOISER_ALPHA_MODE_COPY;
        RT_OPTIX_CHECK(optixDenoiserCreate(context_, OPTIX_DENOISER_MODEL_KIND_TEMPORAL_AOV,
            &options, &denoiser_));

        OptixDenoiserSizes sizes {};
        RT_OPTIX_CHECK(optixDenoiserComputeMemoryResources(denoiser_,
            static_cast<unsigned int>(width_), static_cast<unsigned int>(height_), &sizes));
        state_size_ = sizes.stateSizeInBytes;
        scratch_size_ = sizes.withoutOverlapScratchSizeInBytes;
        internal_pixel_size_ = sizes.internalGuideLayerPixelSizeInBytes;

        const std::size_t pixel_count =
            static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
        allocate_device(state_, state_size_);
        allocate_device(scratch_, scratch_size_);
        allocate_device(output_, pixel_count * sizeof(float4));
        allocate_device(previous_output_, pixel_count * sizeof(float4));
        allocate_device(previous_internal_, pixel_count * internal_pixel_size_);
        allocate_device(output_internal_, pixel_count * internal_pixel_size_);
        allocate_device(hdr_intensity_, sizeof(float));
        RT_CUDA_CHECK(cudaMemsetAsync(reinterpret_cast<void*>(previous_internal_), 0,
            pixel_count * internal_pixel_size_, stream_));

        RT_OPTIX_CHECK(optixDenoiserSetup(denoiser_, stream_, static_cast<unsigned int>(width_),
            static_cast<unsigned int>(height_), state_, state_size_, scratch_, scratch_size_));
    } catch (...) {
        shutdown();
        throw;
    }
}

const float4* OptixDenoiserWrapper::run(OptixDeviceContext context, cudaStream_t stream,
    const DeviceFrameBuffers& frame, int width, int height, float* elapsed_ms) {
    if (frame.beauty == nullptr || frame.albedo == nullptr || frame.denoiser_normal == nullptr
        || frame.flow == nullptr || frame.flow_trustworthiness == nullptr) {
        throw std::invalid_argument(
            "OptiX denoiser requires beauty, albedo, normal, flow, and flow trust guides");
    }
    if (denoiser_ == nullptr || context_ != context || stream_ != stream || width_ != width
        || height_ != height) {
        initialize(context, stream, width, height);
    }

    const unsigned int image_width = static_cast<unsigned int>(width_);
    const unsigned int image_height = static_cast<unsigned int>(height_);
    const std::size_t image_bytes =
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * sizeof(float4);
    const std::size_t internal_bytes =
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * internal_pixel_size_;

    OptixDenoiserGuideLayer guides {};
    guides.albedo = make_image(device_ptr(frame.albedo), image_width, image_height, sizeof(float4),
        OPTIX_PIXEL_FORMAT_FLOAT4);
    guides.normal = make_image(device_ptr(frame.denoiser_normal), image_width, image_height,
        sizeof(float4), OPTIX_PIXEL_FORMAT_FLOAT4);
    guides.flow = make_image(device_ptr(frame.flow), image_width, image_height, sizeof(float2),
        OPTIX_PIXEL_FORMAT_FLOAT2);
    guides.flowTrustworthiness = make_image(device_ptr(frame.flow_trustworthiness), image_width,
        image_height, sizeof(float), OPTIX_PIXEL_FORMAT_FLOAT1);
    guides.previousOutputInternalGuideLayer = make_image(previous_internal_, image_width,
        image_height, static_cast<unsigned int>(internal_pixel_size_),
        OPTIX_PIXEL_FORMAT_INTERNAL_GUIDE_LAYER);
    guides.outputInternalGuideLayer = make_image(output_internal_, image_width, image_height,
        static_cast<unsigned int>(internal_pixel_size_), OPTIX_PIXEL_FORMAT_INTERNAL_GUIDE_LAYER);

    OptixDenoiserLayer layer {};
    layer.input = make_image(device_ptr(frame.beauty), image_width, image_height, sizeof(float4),
        OPTIX_PIXEL_FORMAT_FLOAT4);
    layer.previousOutput = make_image(previous_output_, image_width, image_height, sizeof(float4),
        OPTIX_PIXEL_FORMAT_FLOAT4);
    layer.output =
        make_image(output_, image_width, image_height, sizeof(float4), OPTIX_PIXEL_FORMAT_FLOAT4);
    layer.type = OPTIX_DENOISER_AOV_TYPE_BEAUTY;

    const auto invoke = [&]() {
        if (!has_previous_) {
            RT_CUDA_CHECK(cudaMemcpyAsync(reinterpret_cast<void*>(previous_output_), frame.beauty,
                image_bytes, cudaMemcpyDeviceToDevice, stream_));
            RT_CUDA_CHECK(cudaMemsetAsync(reinterpret_cast<void*>(previous_internal_), 0,
                internal_bytes, stream_));
        }

        RT_OPTIX_CHECK(optixDenoiserComputeIntensity(denoiser_, stream_, &layer.input,
            hdr_intensity_, scratch_, scratch_size_));
        OptixDenoiserParams params {};
        params.hdrIntensity = hdr_intensity_;
        params.temporalModeUsePreviousLayers = has_previous_ ? 1U : 0U;
        RT_OPTIX_CHECK(optixDenoiserInvoke(denoiser_, stream_, &params, state_, state_size_,
            &guides, &layer, 1, 0, 0, scratch_, scratch_size_));
        RT_CUDA_CHECK(cudaMemcpyAsync(reinterpret_cast<void*>(previous_output_),
            reinterpret_cast<void*>(output_), image_bytes, cudaMemcpyDeviceToDevice, stream_));
    };

    if (elapsed_ms == nullptr) {
        invoke();
    } else {
        CudaEventTimer timer(stream_);
        timer.record_start();
        invoke();
        *elapsed_ms = timer.record_stop_and_elapsed_ms();
    }

    std::swap(previous_internal_, output_internal_);
    has_previous_ = true;
    return reinterpret_cast<const float4*>(output_);
}

void OptixDenoiserWrapper::reset_history() {
    has_previous_ = false;
}

void OptixDenoiserWrapper::shutdown() {
    has_previous_ = false;
    free_device(hdr_intensity_);
    free_device(output_internal_);
    free_device(previous_internal_);
    free_device(previous_output_);
    free_device(output_);
    free_device(scratch_);
    free_device(state_);
    if (denoiser_ != nullptr) {
        optixDenoiserDestroy(denoiser_);
        denoiser_ = nullptr;
    }
    context_ = nullptr;
    stream_ = nullptr;
    state_size_ = 0;
    scratch_size_ = 0;
    internal_pixel_size_ = 0;
    width_ = 0;
    height_ = 0;
}

} // namespace rt
