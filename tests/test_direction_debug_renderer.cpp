#include "camera_contract_fixtures.h"
#include "realtime/gpu/direction_debug_renderer.h"
#include "test_support.h"

#include <cuda_runtime.h>

#include <cstddef>

int main() {
    cudaStream_t stream = nullptr;
    cudaStreamCreate(&stream);

    const rt::PackedCamera camera = rt::test::make_contract_test_pinhole_camera();
    const rt::DirectionDebugFrame frame = rt::render_direction_debug_frame(camera, stream);

    expect_true(frame.width == camera.width, "direction debug width");
    expect_true(frame.height == camera.height, "direction debug height");
    expect_true(frame.rgba.size() == static_cast<std::size_t>(camera.width) * static_cast<std::size_t>(camera.height) * 4U,
        "direction debug rgba size");
    expect_true(frame.rgba[3] == 255, "direction debug alpha");

    cudaStreamDestroy(stream);
    return 0;
}
