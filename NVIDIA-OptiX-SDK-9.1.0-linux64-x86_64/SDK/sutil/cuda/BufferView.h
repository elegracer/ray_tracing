/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2025  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/Preprocessor.h>

namespace sutil
{

template <typename T>
struct BufferView
{
    CUdeviceptr    data           CONST_STATIC_INIT( 0 );
    unsigned int   count          CONST_STATIC_INIT( 0 );
    unsigned short byte_stride    CONST_STATIC_INIT( 0 );
    unsigned short elmt_byte_size CONST_STATIC_INIT( 0 );

    SUTIL_HOSTDEVICE bool isValid() const
    { return static_cast<bool>( data ); }

    SUTIL_HOSTDEVICE operator bool() const
    { return isValid(); }

    SUTIL_HOSTDEVICE const T& operator[]( unsigned int idx ) const
    { return *reinterpret_cast<T*>( data + idx*(byte_stride ? byte_stride : sizeof( T ) ) ); }
};

typedef BufferView<unsigned int> GenericBufferView;

}
