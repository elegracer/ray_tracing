/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 - 2024  NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sutil/sutilapi.h>
#include <sutil/vec_math.h>


namespace sutil {

// implementing a perspective camera
class Camera {
public:
    SUTILAPI Camera()
        : m_eye(make_float3(1.0f)), m_lookat(make_float3(0.0f)), m_up(make_float3(0.0f, 1.0f, 0.0f)), m_fovY(35.0f), m_aspectRatio(1.0f)
    {
    }

    SUTILAPI Camera(const float3& eye, const float3& lookat, const float3& up, float fovY, float aspectRatio)
        : m_eye(eye), m_lookat(lookat), m_up(up), m_fovY(fovY), m_aspectRatio(aspectRatio)
    {
    }

    SUTILAPI float3 direction() const { return normalize(m_lookat - m_eye); }
    SUTILAPI void setDirection(const float3& dir) { m_lookat = m_eye + length(m_lookat - m_eye) * dir; }

    SUTILAPI const float3& eye() const { return m_eye; }
    SUTILAPI void setEye(const float3& val) { m_eye = val; }
    SUTILAPI const float3& lookat() const { return m_lookat; }
    SUTILAPI void setLookat(const float3& val) { m_lookat = val; }
    SUTILAPI const float3& up() const { return m_up; }
    SUTILAPI void setUp(const float3& val) { m_up = val; }
    SUTILAPI const float& fovY() const { return m_fovY; }
    SUTILAPI void setFovY(const float& val) { m_fovY = val; }
    SUTILAPI const float& aspectRatio() const { return m_aspectRatio; }
    SUTILAPI void setAspectRatio(const float& val) { m_aspectRatio = val; }

    // UVW forms an orthogonal, but not orthonormal basis!
    SUTILAPI void UVWFrame(float3& U, float3& V, float3& W) const;

private:
    float3 m_eye;
    float3 m_lookat;
    float3 m_up;
    float m_fovY;
    float m_aspectRatio;
};

}
