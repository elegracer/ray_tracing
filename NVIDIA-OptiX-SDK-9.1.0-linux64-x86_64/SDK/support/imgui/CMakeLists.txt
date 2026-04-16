#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

add_library( imgui SHARED
  imconfig.h
  imgui.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui.h
  imgui_impl_glfw.cpp
  imgui_impl_glfw.h
  imgui_impl_opengl3.cpp
  imgui_impl_opengl3.h
  imgui_internal.h
  imgui_widgets.cpp
  imstb_rectpack.h
  imstb_textedit.h
  imstb_truetype.h
  )

target_include_directories( imgui
  INTERFACE ..
  PRIVATE . )
target_compile_definitions( imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD )

# On Windows, define IMGUI_API for public DLL symbols.
if( WIN32 )
  # Mark functions as exports when compiling shared library
  target_compile_definitions( imgui PRIVATE "IMGUI_API=__declspec(dllexport)" )

  # Mark functions as imports when linking against the library
  target_compile_definitions( imgui INTERFACE "IMGUI_API=__declspec(dllimport)" )
else()
  target_compile_definitions( imgui PUBLIC "IMGUI_API=__attribute__ ((visibility (\"default\")))" )
endif()

target_link_libraries( imgui glfw glad ${OPENGL_gl_LIBRARY})
