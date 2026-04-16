/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <optix.h>
#include <optix_stubs.h>

#include "Hair.h"
#include "optixHair.h"

#include <sutil/Trackball.h>

#include <string>


// forward declarations
struct GLFWwindow;

class Renderer
{
  public:
    Renderer( HairState* pState );

    Camera defaultCamera() const;

  protected:
    void             render() const;
    HairState* const m_pState;
};

class FileRenderer : public Renderer
{
  public:
    FileRenderer( HairState* pState );

    void renderFile( const std::string& fileName ) const;
};

class WindowRenderer : public Renderer
{
  public:
    WindowRenderer( HairState* pState );

    ~WindowRenderer();

    void run();

  protected:
    //
    // GLFW callbacks
    //
    static void mouseButtonCallback( GLFWwindow* window, int button, int action, int mods );
    static void cursorPosCallback( GLFWwindow* window, double xpos, double ypos );
    static void windowSizeCallback( GLFWwindow* window, int32_t res_x, int32_t res_y );
    static void windowIconifyCallback( GLFWwindow* window, int32_t iconified );
    static void keyCallback( GLFWwindow* window, int32_t key, int32_t /*scancode*/, int32_t action, int32_t /*mods*/ );
    static void scrollCallback( GLFWwindow* window, double xscroll, double yscroll );

  private:
    void                   displayHairStats( std::chrono::duration<double>& state_update_time,
                                             std::chrono::duration<double>& render_time,
                                             std::chrono::duration<double>& display_time,
                                             const Hair::SplineMode         splineMode );
    static WindowRenderer* GetRenderer( GLFWwindow* window );
    GLFWwindow*            m_window        = nullptr;
    sutil::Trackball       m_trackball     = {};
    int32_t                m_mouseButton   = -1;
    bool                   m_minimized     = false;
};
