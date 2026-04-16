#
# SPDX-FileCopyrightText: Copyright (c) 2021 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
  ${SAMPLES_DIR}/lib/NanoVDB/include/)

OPTIX_add_sample_executable( optixVolumeViewer target_name
  optixVolumeViewer.h
  optixVolumeViewer.cpp
  volume.h
  volume_cuda.h
  volume.cu

  nanovdb/NanoVDB.h
  nanovdb/util/GridHandle.h
  nanovdb/util/HDDA.h
  nanovdb/util/HostBuffer.h
  nanovdb/util/IO.h
  nanovdb/util/Ray.h

  OPTIONS -rdc true
  )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  )
