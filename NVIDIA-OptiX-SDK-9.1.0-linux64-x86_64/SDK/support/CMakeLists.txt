#
# SPDX-FileCopyrightText: Copyright (c) 2019 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#

find_package( OpenGL REQUIRED )

if( UNIX )
  # On Unix-like systems, shared libraries can use the soname system.
  set(GLFW_LIB_NAME glfw)
else()
  set(GLFW_LIB_NAME glfw3)
endif()

# Disable unnecessary GLFW components.
set(GLFW_BUILD_DOCS OFF)
set(GLFW_BUILD_WAYLAND OFF)

# Filter out warnings that cause problems with GLFW.
if( WARNINGS_AS_ERRORS AND (USING_GNU_CXX OR USING_CLANG_CXX))
  string( REPLACE "-Wdeclaration-after-statement" "" filtered_c_flags ${CMAKE_C_FLAGS} )
  string( REPLACE "-Wsign-compare" "" filtered_c_flags ${filtered_c_flags} )
  push_variable( CMAKE_C_FLAGS "${filtered_c_flags} -Wno-format-truncation -Wno-deprecated" )
endif()
add_subdirectory( glfw-3.4 )
set_property( TARGET glfw PROPERTY C_STANDARD 99 )
if( WARNINGS_AS_ERRORS AND (USING_GNU_CXX OR USING_CLANG_CXX))
  pop_variable( CMAKE_C_FLAGS )
endif()

if( WARNINGS_AS_ERRORS AND (USING_GNU_CXX OR USING_CLANG_CXX))
    push_variable( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-class-memaccess" )
endif()
add_subdirectory( imgui )
if( WARNINGS_AS_ERRORS AND (USING_GNU_CXX OR USING_CLANG_CXX))
  pop_variable( CMAKE_CXX_FLAGS )
endif()

add_subdirectory( miniz )

add_library( glad SHARED
  KHR/khrplatform.h
  glad/glad.c
  glad/glad.h
)
target_compile_definitions( glad
  PRIVATE GLAD_GLAPI_EXPORT_BUILD
  PUBLIC  GLAD_GLAPI_EXPORT )
target_include_directories( glad PUBLIC . )
target_link_libraries( glad PUBLIC ${OPENGL_LIBRARIES} )


# Set IDE folders for targets
set_property( TARGET glad PROPERTY FOLDER "${OPTIX_IDE_FOLDER}/libraries" )
set_property( TARGET glfw PROPERTY FOLDER "${OPTIX_IDE_FOLDER}/libraries" )
set_property( TARGET imgui PROPERTY FOLDER "${OPTIX_IDE_FOLDER}/libraries" )
set_property( TARGET miniz PROPERTY FOLDER "${OPTIX_IDE_FOLDER}/libraries" )

set_target_properties(update_mappings PROPERTIES FOLDER "${OPTIX_IDE_FOLDER}/libraries/GLFW3")
set_target_properties(uninstall PROPERTIES FOLDER "${OPTIX_IDE_FOLDER}/libraries/GLFW3")
