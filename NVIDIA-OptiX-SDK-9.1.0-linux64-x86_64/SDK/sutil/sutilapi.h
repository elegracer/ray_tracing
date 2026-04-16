/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#ifndef SUTILAPI
#  if sutil_7_sdk_EXPORTS /* Set by CMAKE */
#    if defined( _WIN32 ) || defined( _WIN64 )
#      define SUTILAPI __declspec(dllexport)
#      define SUTILCLASSAPI
#    elif defined( linux ) || defined( __linux__ ) || defined ( __CYGWIN__ )
#      define SUTILAPI __attribute__ ((visibility ("default")))
#      define SUTILCLASSAPI SUTILAPI
#    elif defined( __APPLE__ ) && defined( __MACH__ )
#      define SUTILAPI __attribute__ ((visibility ("default")))
#      define SUTILCLASSAPI SUTILAPI
#    else
#      error "CODE FOR THIS OS HAS NOT YET BEEN DEFINED"
#    endif

#  else /* sutil_7_sdk_EXPORTS */

#    if defined( _WIN32 ) || defined( _WIN64 )
#      define SUTILAPI __declspec(dllimport)
#      define SUTILCLASSAPI
#    elif defined( linux ) || defined( __linux__ ) || defined ( __CYGWIN__ )
#      define SUTILAPI __attribute__ ((visibility ("default")))
#      define SUTILCLASSAPI SUTILAPI
#    elif defined( __APPLE__ ) && defined( __MACH__ )
#      define SUTILAPI __attribute__ ((visibility ("default")))
#      define SUTILCLASSAPI SUTILAPI
#    elif defined( __CUDACC_RTC__ )
#      define SUTILAPI
#      define SUTILCLASSAPI
#    else
#      error "CODE FOR THIS OS HAS NOT YET BEEN DEFINED"
#    endif

#  endif /* sutil_7_sdk_EXPORTS */
#endif
