#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

# libntc enforces cmake 3.18 - skip this sample gracefully if your cmake is older
if (CMAKE_VERSION VERSION_LESS 3.18)
  message(WARNING "CMake version 3.18 or greater is needed for optixNeuralTexture. Skipping.")
  return()
endif()


# libntc requires c++17
set( CMAKE_CXX_STANDARD 17 )


#-----------------
# Build NTC library with the following options

set( NTC_WITH_CUDA OFF CACHE BOOL "" )
set( NTC_WITH_DX12 OFF CACHE BOOL "" )
set( NTC_WITH_VULKAN OFF CACHE BOOL "" )
set( NTC_BUILD_SHARED FALSE )

# libntc code is currently embedded in this sample, as it is not public at the time of writing.
# In the future, this sample may link to and automatically download the public version of libntc.
# Find the latest version of the NTC SDK on Github, including NTC explorer and DX, Vulkan, CUDA, and Slang examples.
add_subdirectory( libntc )
set_property(TARGET libntc PROPERTY FOLDER "${OPTIX_IDE_FOLDER}/libraries")

#-----------------

set_target_properties(libntc PROPERTIES COMPILE_WARNING_AS_ERROR FALSE)
target_compile_options(libntc PRIVATE -w)

include_directories(${libntc_SOURCE_DIR}/include)

# nuke all the arch flags here (so we can override)
list(FIND SAMPLES_NVCC_FLAGS "-arch" index )
while( index GREATER -1 )
  list(REMOVE_ITEM SAMPLES_NVCC_FLAGS "-arch")
  list(REMOVE_AT SAMPLES_NVCC_FLAGS ${index})
  list(FIND SAMPLES_NVCC_FLAGS "-arch" index )
endwhile()

compare_sm_value(${CUDA_MIN_SM_TARGET} 60 TARGET_SM_VERSION)

OPTIX_add_sample_executable( optixNeuralTexture target_name
  NtcInference.h
  NtcTexture.h
  NtcTextureSet.h
  optixNeuralTexture.cpp
  optixNeuralTextureKernel.cu
  optixNeuralTexture.h
  optixNeuralTextureUtil.h
  OPTIONS -arch ${TARGET_SM_VERSION} -rdc true --std=c++17
  )

target_link_libraries( ${target_name}
  ${CUDA_LIBRARIES}
  libntc
  )
