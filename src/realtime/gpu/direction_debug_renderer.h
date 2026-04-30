#pragma once

#include "realtime/camera_rig.h"
#include "realtime/gpu/frame_types.h"

#include <cuda_runtime.h>

namespace rt {

DirectionDebugFrame render_direction_debug_frame(const PackedCamera& camera, cudaStream_t stream);

}  // namespace rt
