#include "realtime/gpu/renderer_pool.h"

#include "realtime/gpu/render_request_validation.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace rt {

namespace {

class RendererWorker {
public:
    RendererWorker(std::shared_ptr<SharedGpuSceneState> shared_scene,
        std::atomic<std::uint64_t>& worker_starts, std::atomic<std::uint64_t>& submissions)
        : renderer_(std::move(shared_scene)),
          worker_starts_(worker_starts),
          submissions_(submissions),
          thread_([this]() { run(); }) {}

    ~RendererWorker() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        condition_.notify_one();
        thread_.join();
    }

    RendererWorker(const RendererWorker&) = delete;
    RendererWorker& operator=(const RendererWorker&) = delete;

    template<typename Fn>
    auto submit(Fn&& fn, bool record_submission = true) {
        using Result = std::invoke_result_t<Fn, OptixRenderer&>;
        auto task = std::make_shared<std::packaged_task<Result()>>(
            [this, function = std::forward<Fn>(fn)]() mutable {
                return std::invoke(function, renderer_);
            });
        std::future<Result> future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("renderer worker is stopping");
            }
            tasks_.emplace_back([task]() { (*task)(); });
            if (record_submission) {
                submissions_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        condition_.notify_one();
        return future;
    }

private:
    void run() {
        worker_starts_.fetch_add(1, std::memory_order_relaxed);
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            task();
        }
    }

    OptixRenderer renderer_;
    std::atomic<std::uint64_t>& worker_starts_;
    std::atomic<std::uint64_t>& submissions_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::function<void()>> tasks_;
    bool stopping_ = false;
    std::thread thread_;
};

} // namespace

struct RendererPool::Impl {
    explicit Impl(int renderer_count) : shared_scene(std::make_shared<SharedGpuSceneState>()) {
        workers.reserve(static_cast<std::size_t>(renderer_count));
        for (int i = 0; i < renderer_count; ++i) {
            workers.push_back(
                std::make_unique<RendererWorker>(shared_scene, worker_starts, submissions));
        }
    }

    std::shared_ptr<SharedGpuSceneState> shared_scene;
    std::vector<std::unique_ptr<RendererWorker>> workers;
    std::atomic<std::uint64_t> worker_starts {0};
    std::atomic<std::uint64_t> submissions {0};
    mutable std::mutex operation_mutex;
    AccelerationUpdateStats acceleration;
};

RendererPool::RendererPool(int renderer_count) {
    if (renderer_count < 1 || renderer_count > 4) {
        throw std::runtime_error("RendererPool requires renderer_count in [1, 4]");
    }
    impl_ = std::make_unique<Impl>(renderer_count);
}

RendererPool::~RendererPool() = default;

void RendererPool::prepare_scene(const PackedScene& scene) {
    std::lock_guard<std::mutex> lock(impl_->operation_mutex);
    impl_->acceleration = impl_->shared_scene->prepare(scene);
    std::vector<std::future<void>> futures;
    futures.reserve(impl_->workers.size());
    for (const std::unique_ptr<RendererWorker>& worker : impl_->workers) {
        futures.push_back(worker->submit(
            [&scene](OptixRenderer& renderer) { renderer.use_prepared_scene(scene); }));
    }
    for (std::future<void>& future : futures) {
        future.get();
    }
}

void RendererPool::reset_accumulation() {
    std::lock_guard<std::mutex> lock(impl_->operation_mutex);
    std::vector<std::future<void>> futures;
    futures.reserve(impl_->workers.size());
    for (const std::unique_ptr<RendererWorker>& worker : impl_->workers) {
        futures.push_back(
            worker->submit([](OptixRenderer& renderer) { renderer.reset_accumulation(); }));
    }
    for (std::future<void>& future : futures) {
        future.get();
    }
}

void RendererPool::reset_sequence(std::uint32_t sample_stream) {
    std::lock_guard<std::mutex> lock(impl_->operation_mutex);
    std::vector<std::future<void>> futures;
    futures.reserve(impl_->workers.size());
    for (const std::unique_ptr<RendererWorker>& worker : impl_->workers) {
        futures.push_back(worker->submit(
            [sample_stream](OptixRenderer& renderer) { renderer.reset_sequence(sample_stream); }));
    }
    for (std::future<void>& future : futures) {
        future.get();
    }
}

std::vector<CameraRenderResult> RendererPool::render_frame(const PackedCameraRig& rig,
    const RenderProfile& profile, int active_cameras) {
    std::lock_guard<std::mutex> lock(impl_->operation_mutex);
    validate_render_pool_request(rig, active_cameras, static_cast<int>(impl_->workers.size()),
        "RendererPool");

    std::vector<CameraRenderResult> results;
    results.reserve(static_cast<std::size_t>(active_cameras));

    std::vector<std::future<CameraRenderResult>> futures;
    futures.reserve(static_cast<std::size_t>(active_cameras));
    for (int camera_index = 0; camera_index < active_cameras; ++camera_index) {
        futures.push_back(impl_->workers[static_cast<std::size_t>(camera_index)]->submit(
            [&rig, &profile, camera_index](OptixRenderer& renderer) {
                CameraRenderResult result {};
                result.camera_index = camera_index;
                result.profiled = renderer.render_prepared_radiance(rig, profile, camera_index);
                return result;
            }));
    }

    for (std::future<CameraRenderResult>& future : futures) {
        results.push_back(future.get());
    }
    return results;
}

std::vector<CameraDeviceRenderResult> RendererPool::render_device_frame(const PackedCameraRig& rig,
    const RenderProfile& profile, int active_cameras) {
    std::lock_guard<std::mutex> lock(impl_->operation_mutex);
    validate_render_pool_request(rig, active_cameras, static_cast<int>(impl_->workers.size()),
        "RendererPool");

    std::vector<CameraDeviceRenderResult> results;
    results.reserve(static_cast<std::size_t>(active_cameras));

    std::vector<std::future<CameraDeviceRenderResult>> futures;
    futures.reserve(static_cast<std::size_t>(active_cameras));
    for (int camera_index = 0; camera_index < active_cameras; ++camera_index) {
        futures.push_back(impl_->workers[static_cast<std::size_t>(camera_index)]->submit(
            [&rig, &profile, camera_index](OptixRenderer& renderer) {
                CameraDeviceRenderResult result {};
                result.camera_index = camera_index;
                result.profiled = renderer.render_prepared_device(rig, profile, camera_index);
                return result;
            }));
    }

    for (std::future<CameraDeviceRenderResult>& future : futures) {
        results.push_back(future.get());
    }
    return results;
}

RendererPoolDiagnostics RendererPool::diagnostics() const {
    std::lock_guard<std::mutex> lock(impl_->operation_mutex);
    std::uint64_t launch_parameter_allocation_count = 0;
    std::uint64_t launch_parameter_upload_count = 0;
    std::vector<std::future<LaunchParameterDiagnostics>> futures;
    futures.reserve(impl_->workers.size());
    for (const std::unique_ptr<RendererWorker>& worker : impl_->workers) {
        futures.push_back(worker->submit(
            [](OptixRenderer& renderer) { return renderer.launch_parameter_diagnostics(); },
            false));
    }
    for (std::future<LaunchParameterDiagnostics>& future : futures) {
        const LaunchParameterDiagnostics diagnostics = future.get();
        launch_parameter_allocation_count += diagnostics.allocation_count;
        launch_parameter_upload_count += diagnostics.upload_count;
    }
    return RendererPoolDiagnostics {
        .worker_count = static_cast<int>(impl_->workers.size()),
        .worker_start_count = impl_->worker_starts.load(std::memory_order_relaxed),
        .task_submission_count = impl_->submissions.load(std::memory_order_relaxed),
        .launch_parameter_allocation_count = launch_parameter_allocation_count,
        .launch_parameter_upload_count = launch_parameter_upload_count,
        .acceleration = impl_->acceleration,
    };
}

} // namespace rt
