#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
# 

add_library( miniz STATIC
  miniz.c
  )

target_include_directories( miniz
  INTERFACE ..
  PRIVATE . )
target_compile_definitions( miniz PRIVATE )
target_link_libraries( miniz )
