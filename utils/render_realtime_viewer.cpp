#include "realtime/gpu/renderer_pool.h"
#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "realtime/viewer/four_camera_rig.h"

#include <GLFW/glfw3.h>
#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 960;
constexpr int kViewWidth = 640;
constexpr int kViewHeight = 480;
constexpr double kLookDegreesPerPixel = 0.08;
constexpr double kMoveSpeedUnitsPerSecond = 1.8;

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

std::vector<std::uint8_t> to_rgba8(const rt::RadianceFrame& frame) {
    const int pixel_count = frame.width * frame.height;
    const std::size_t expected = static_cast<std::size_t>(pixel_count) * 4;

    std::vector<std::uint8_t> rgba8(expected, 0);
    if (frame.width <= 0 || frame.height <= 0 || frame.beauty_rgba.size() < expected) {
        return rgba8;
    }

    for (int p = 0; p < pixel_count; ++p) {
        const float r = clamp01(frame.beauty_rgba[static_cast<std::size_t>(p) * 4 + 0]);
        const float g = clamp01(frame.beauty_rgba[static_cast<std::size_t>(p) * 4 + 1]);
        const float b = clamp01(frame.beauty_rgba[static_cast<std::size_t>(p) * 4 + 2]);

        rgba8[static_cast<std::size_t>(p) * 4 + 0] =
            static_cast<std::uint8_t>(std::lround(r * 255.0f));
        rgba8[static_cast<std::size_t>(p) * 4 + 1] =
            static_cast<std::uint8_t>(std::lround(g * 255.0f));
        rgba8[static_cast<std::size_t>(p) * 4 + 2] =
            static_cast<std::uint8_t>(std::lround(b * 255.0f));
        rgba8[static_cast<std::size_t>(p) * 4 + 3] = 255;
    }

    return rgba8;
}

void upload_texture(GLuint texture, const rt::RadianceFrame& frame) {
    const std::vector<std::uint8_t> rgba8 = to_rgba8(frame);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0, frame.width, frame.height, GL_RGBA, GL_UNSIGNED_BYTE, rgba8.data());
}

void draw_textured_quad(GLuint texture, float x, float y, float width, float height,
    float window_width, float window_height) {
    glBindTexture(GL_TEXTURE_2D, texture);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, window_width, window_height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x, y);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x + width, y);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x + width, y + height);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x, y + height);
    glEnd();
}

}  // namespace

int main() {
    if (!glfwInit()) {
        fmt::print(stderr, "glfwInit failed\n");
        return 1;
    }

    GLFWwindow* window =
        glfwCreateWindow(kWindowWidth, kWindowHeight, "render_realtime_viewer", nullptr, nullptr);
    if (!window) {
        fmt::print(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    std::array<GLuint, 4> textures{};
    glGenTextures(static_cast<GLsizei>(textures.size()), textures.data());
    for (GLuint texture : textures) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kViewWidth, kViewHeight, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, nullptr);
    }

    rt::viewer::BodyPose pose = rt::viewer::default_spawn_pose();
    const rt::PackedScene scene = rt::viewer::make_default_viewer_scene().pack();
    const rt::RenderProfile profile = rt::viewer::default_viewer_profile();

    rt::RendererPool pool(4);
    pool.prepare_scene(scene);

    double last_time = glfwGetTime();
    double last_cursor_x = 0.0;
    double last_cursor_y = 0.0;
    glfwGetCursorPos(window, &last_cursor_x, &last_cursor_y);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const double now = glfwGetTime();
        double dt = now - last_time;
        last_time = now;
        dt = std::clamp(dt, 0.0, 0.1);

        double cursor_x = 0.0;
        double cursor_y = 0.0;
        glfwGetCursorPos(window, &cursor_x, &cursor_y);
        const double delta_x = cursor_x - last_cursor_x;
        const double delta_y = cursor_y - last_cursor_y;
        last_cursor_x = cursor_x;
        last_cursor_y = cursor_y;

        rt::viewer::integrate_mouse_look(pose, delta_x, delta_y, kLookDegreesPerPixel);

        rt::viewer::integrate_wasd(pose, glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS, kMoveSpeedUnitsPerSecond * dt);

        const rt::PackedCameraRig rig = rt::viewer::make_default_viewer_rig(pose, kViewWidth, kViewHeight).pack();
        const std::vector<rt::CameraRenderResult> results = pool.render_frame(rig, profile, 4);
        for (const auto& result : results) {
            const int idx = result.camera_index;
            if (idx < 0 || idx >= static_cast<int>(textures.size())) {
                continue;
            }
            upload_texture(textures[static_cast<std::size_t>(idx)], result.profiled.frame);
        }

        glViewport(0, 0, kWindowWidth, kWindowHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        draw_textured_quad(textures[0], 0.0f, 0.0f, static_cast<float>(kViewWidth),
            static_cast<float>(kViewHeight), static_cast<float>(kWindowWidth),
            static_cast<float>(kWindowHeight));
        draw_textured_quad(textures[1], static_cast<float>(kViewWidth), 0.0f,
            static_cast<float>(kViewWidth), static_cast<float>(kViewHeight),
            static_cast<float>(kWindowWidth), static_cast<float>(kWindowHeight));
        draw_textured_quad(textures[2], 0.0f, static_cast<float>(kViewHeight),
            static_cast<float>(kViewWidth), static_cast<float>(kViewHeight),
            static_cast<float>(kWindowWidth), static_cast<float>(kWindowHeight));
        draw_textured_quad(textures[3], static_cast<float>(kViewWidth),
            static_cast<float>(kViewHeight), static_cast<float>(kViewWidth),
            static_cast<float>(kViewHeight), static_cast<float>(kWindowWidth),
            static_cast<float>(kWindowHeight));

        glfwSwapBuffers(window);
    }

    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

