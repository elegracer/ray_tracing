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
constexpr float kLabelGlyphWidth = 1.0f;
constexpr float kLabelGlyphHeight = 1.6f;
constexpr float kLabelGlyphAdvance = 1.35f;
constexpr float kLabelPadX = 0.35f;
constexpr float kLabelPadY = 0.25f;

constexpr std::array<const char*, 4> kCameraLabels {"cam0", "cam1", "cam2", "cam3"};

struct ViewerPanel {
    int camera_index;
    int column;
    int row;
};

enum class LabelCorner {
    top_left,
    top_right,
    bottom_left,
    bottom_right,
};

constexpr std::array<ViewerPanel, 4> kViewerPanels {{
    {.camera_index = 0, .column = 0, .row = 0},
    {.camera_index = 1, .column = 1, .row = 0},
    {.camera_index = 3, .column = 0, .row = 1},
    {.camera_index = 2, .column = 1, .row = 1},
}};

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

constexpr float label_text_width() {
    return 3.0f * kLabelGlyphAdvance + kLabelGlyphWidth;
}

void emit_line_segment(float x0, float y0, float x1, float y1, float x_offset) {
    glVertex2f(x_offset + x0, y0);
    glVertex2f(x_offset + x1, y1);
}

void emit_glyph(char glyph, float x_offset) {
    switch (glyph) {
    case 'c':
        emit_line_segment(1.0f, 0.0f, 0.0f, 0.0f, x_offset);
        emit_line_segment(0.0f, 0.0f, 0.0f, kLabelGlyphHeight, x_offset);
        emit_line_segment(0.0f, kLabelGlyphHeight, 1.0f, kLabelGlyphHeight, x_offset);
        break;
    case 'a':
        emit_line_segment(0.0f, kLabelGlyphHeight, 0.0f, 0.55f, x_offset);
        emit_line_segment(0.0f, 0.55f, 0.25f, 0.0f, x_offset);
        emit_line_segment(0.25f, 0.0f, 0.75f, 0.0f, x_offset);
        emit_line_segment(0.75f, 0.0f, 1.0f, 0.55f, x_offset);
        emit_line_segment(1.0f, 0.55f, 1.0f, kLabelGlyphHeight, x_offset);
        emit_line_segment(0.0f, 0.85f, 1.0f, 0.85f, x_offset);
        break;
    case 'm':
        emit_line_segment(0.0f, kLabelGlyphHeight, 0.0f, 0.0f, x_offset);
        emit_line_segment(0.0f, 0.0f, 0.5f, 0.7f, x_offset);
        emit_line_segment(0.5f, 0.7f, 1.0f, 0.0f, x_offset);
        emit_line_segment(1.0f, 0.0f, 1.0f, kLabelGlyphHeight, x_offset);
        break;
    case '0':
        emit_line_segment(0.0f, 0.0f, 1.0f, 0.0f, x_offset);
        emit_line_segment(1.0f, 0.0f, 1.0f, kLabelGlyphHeight, x_offset);
        emit_line_segment(1.0f, kLabelGlyphHeight, 0.0f, kLabelGlyphHeight, x_offset);
        emit_line_segment(0.0f, kLabelGlyphHeight, 0.0f, 0.0f, x_offset);
        break;
    case '1':
        emit_line_segment(0.5f, 0.0f, 0.5f, kLabelGlyphHeight, x_offset);
        emit_line_segment(0.2f, 0.3f, 0.5f, 0.0f, x_offset);
        emit_line_segment(0.2f, kLabelGlyphHeight, 0.8f, kLabelGlyphHeight, x_offset);
        break;
    case '2':
        emit_line_segment(0.0f, 0.0f, 1.0f, 0.0f, x_offset);
        emit_line_segment(1.0f, 0.0f, 1.0f, 0.8f, x_offset);
        emit_line_segment(1.0f, 0.8f, 0.0f, 0.8f, x_offset);
        emit_line_segment(0.0f, 0.8f, 0.0f, kLabelGlyphHeight, x_offset);
        emit_line_segment(0.0f, kLabelGlyphHeight, 1.0f, kLabelGlyphHeight, x_offset);
        break;
    case '3':
        emit_line_segment(0.0f, 0.0f, 1.0f, 0.0f, x_offset);
        emit_line_segment(1.0f, 0.0f, 1.0f, kLabelGlyphHeight, x_offset);
        emit_line_segment(0.0f, 0.8f, 1.0f, 0.8f, x_offset);
        emit_line_segment(0.0f, kLabelGlyphHeight, 1.0f, kLabelGlyphHeight, x_offset);
        break;
    default:
        break;
    }
}

GLuint build_camera_label_lists() {
    const GLuint list_base = glGenLists(static_cast<GLsizei>(kCameraLabels.size()));
    if (list_base == 0) {
        return 0;
    }

    for (std::size_t i = 0; i < kCameraLabels.size(); ++i) {
        glNewList(list_base + static_cast<GLuint>(i), GL_COMPILE);
        glBegin(GL_LINES);
        const char* label = kCameraLabels[i];
        for (int j = 0; label[j] != '\0'; ++j) {
            emit_glyph(label[j], static_cast<float>(j) * kLabelGlyphAdvance);
        }
        glEnd();
        glEndList();
    }
    return list_base;
}

void draw_camera_label(GLuint label_list, float x, float y, float width, float height, LabelCorner corner) {
    const float unit_scale = std::max(14.0f, std::min(width, height) * 0.05f);
    const float text_width = label_text_width() * unit_scale;
    const float text_height = kLabelGlyphHeight * unit_scale;
    const float box_width = text_width + 2.0f * kLabelPadX * unit_scale;
    const float box_height = text_height + 2.0f * kLabelPadY * unit_scale;
    const float inset = std::max(8.0f, std::min(width, height) * 0.03f);

    float box_left = x + 0.5f * (width - box_width);
    float box_top = y + 0.5f * (height - box_height);
    switch (corner) {
    case LabelCorner::top_left:
        box_left = x + inset;
        box_top = y + inset;
        break;
    case LabelCorner::top_right:
        box_left = x + width - inset - box_width;
        box_top = y + inset;
        break;
    case LabelCorner::bottom_left:
        box_left = x + inset;
        box_top = y + height - inset - box_height;
        break;
    case LabelCorner::bottom_right:
        box_left = x + width - inset - box_width;
        box_top = y + height - inset - box_height;
        break;
    }

    glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
    glBegin(GL_QUADS);
    glVertex2f(box_left, box_top);
    glVertex2f(box_left + box_width, box_top);
    glVertex2f(box_left + box_width, box_top + box_height);
    glVertex2f(box_left, box_top + box_height);
    glEnd();

    glPushMatrix();
    glTranslatef(box_left + kLabelPadX * unit_scale, box_top + kLabelPadY * unit_scale, 0.0f);
    glScalef(unit_scale, unit_scale, 1.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
    glCallList(label_list);
    glPopMatrix();
}

enum class UploadStatus {
    ok,
    resolution_mismatch,
    incomplete_frame,
};

UploadStatus to_rgba8(const rt::RadianceFrame& frame, std::vector<std::uint8_t>& rgba8) {
    if (frame.width != kViewWidth || frame.height != kViewHeight) {
        return UploadStatus::resolution_mismatch;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
    const std::size_t expected = pixel_count * 4;
    if (frame.beauty_rgba.size() < expected) {
        return UploadStatus::incomplete_frame;
    }

    rgba8.resize(expected);
    for (std::size_t p = 0; p < pixel_count; ++p) {
        const float r = clamp01(frame.beauty_rgba[p * 4 + 0]);
        const float g = clamp01(frame.beauty_rgba[p * 4 + 1]);
        const float b = clamp01(frame.beauty_rgba[p * 4 + 2]);

        rgba8[p * 4 + 0] = static_cast<std::uint8_t>(std::lround(r * 255.0f));
        rgba8[p * 4 + 1] = static_cast<std::uint8_t>(std::lround(g * 255.0f));
        rgba8[p * 4 + 2] = static_cast<std::uint8_t>(std::lround(b * 255.0f));
        rgba8[p * 4 + 3] = 255;
    }

    return UploadStatus::ok;
}

UploadStatus upload_texture(GLuint texture, const rt::RadianceFrame& frame, std::vector<std::uint8_t>& scratch) {
    const UploadStatus status = to_rgba8(frame, scratch);
    if (status != UploadStatus::ok) {
        return status;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kViewWidth, kViewHeight, GL_RGBA, GL_UNSIGNED_BYTE, scratch.data());
    return UploadStatus::ok;
}

void draw_textured_quad(GLuint texture, float x, float y, float width, float height) {
    glBindTexture(GL_TEXTURE_2D, texture);

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

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
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
    std::array<std::vector<std::uint8_t>, 4> texture_scratch{};
    std::array<bool, 4> warned_texture_mismatch{};
    const GLuint label_list_base = build_camera_label_lists();
    glGenTextures(static_cast<GLsizei>(textures.size()), textures.data());
    for (GLuint texture : textures) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        if (framebuffer_width <= 0 || framebuffer_height <= 0) {
            glfwWaitEventsTimeout(0.05);
            continue;
        }

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
            glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS,
            kMoveSpeedUnitsPerSecond * dt);

        const rt::PackedCameraRig rig = rt::viewer::make_default_viewer_rig(pose, kViewWidth, kViewHeight).pack();
        const std::vector<rt::CameraRenderResult> results =
            pool.render_frame(rig, profile, static_cast<int>(textures.size()));
        for (const auto& result : results) {
            const int idx = result.camera_index;
            if (idx < 0 || idx >= static_cast<int>(textures.size())) {
                continue;
            }
            const UploadStatus upload_status = upload_texture(
                textures[static_cast<std::size_t>(idx)],
                result.profiled.frame,
                texture_scratch[static_cast<std::size_t>(idx)]);
            if (upload_status == UploadStatus::ok) {
                warned_texture_mismatch[static_cast<std::size_t>(idx)] = false;
                continue;
            }

            if (!warned_texture_mismatch[static_cast<std::size_t>(idx)]) {
                if (upload_status == UploadStatus::resolution_mismatch) {
                    fmt::print(stderr,
                        "viewer frame size mismatch for camera {}: got {}x{}, expected {}x{}\n",
                        idx, result.profiled.frame.width, result.profiled.frame.height, kViewWidth, kViewHeight);
                } else {
                    fmt::print(stderr,
                        "viewer frame buffer too small for camera {}: got {}, need at least {}\n",
                        idx, result.profiled.frame.beauty_rgba.size(),
                        static_cast<std::size_t>(kViewWidth * kViewHeight * 4));
                }
                warned_texture_mismatch[static_cast<std::size_t>(idx)] = true;
            }
        }

        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const float panel_width = 0.5f * static_cast<float>(framebuffer_width);
        const float panel_height = 0.5f * static_cast<float>(framebuffer_height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, static_cast<double>(framebuffer_width), static_cast<double>(framebuffer_height), 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        for (const ViewerPanel& panel : kViewerPanels) {
            const float x = static_cast<float>(panel.column) * panel_width;
            const float y = static_cast<float>(panel.row) * panel_height;
            draw_textured_quad(textures[static_cast<std::size_t>(panel.camera_index)], x, y, panel_width, panel_height);
        }

        if (label_list_base != 0) {
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glLineWidth(2.0f);
            for (const ViewerPanel& panel : kViewerPanels) {
                const float x = static_cast<float>(panel.column) * panel_width;
                const float y = static_cast<float>(panel.row) * panel_height;
                const LabelCorner corner = panel.row == 0
                    ? (panel.column == 0 ? LabelCorner::bottom_right : LabelCorner::bottom_left)
                    : (panel.column == 0 ? LabelCorner::top_right : LabelCorner::top_left);
                draw_camera_label(label_list_base + static_cast<GLuint>(panel.camera_index), x, y, panel_width,
                    panel_height, corner);
            }
            glDisable(GL_BLEND);
            glEnable(GL_TEXTURE_2D);
        }

        glfwSwapBuffers(window);
    }

    if (label_list_base != 0) {
        glDeleteLists(label_list_base, static_cast<GLsizei>(kCameraLabels.size()));
    }
    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
