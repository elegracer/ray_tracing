#include "realtime/render_profile.h"
#include "realtime/realtime_scene_factory.h"
#include "realtime/scene_catalog.h"
#include "realtime/gpu/renderer_pool.h"
#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "realtime/viewer/four_camera_rig.h"
#include "realtime/viewer/viewer_quality_controller.h"
#include "realtime/viewer/scene_switch_controller.h"

#include <GLFW/glfw3.h>
#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
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

UploadStatus to_rgba8(const rt::viewer::ResolvedBeautyFrameView& frame, std::vector<std::uint8_t>& rgba8) {
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

UploadStatus upload_texture(GLuint texture,
    const rt::viewer::ResolvedBeautyFrameView& frame,
    std::vector<std::uint8_t>& scratch) {
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

bool is_supported_realtime_scene(const std::string& scene_id) {
    const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(scene_id);
    return entry != nullptr && entry->supports_realtime;
}

std::string scene_label(std::string_view scene_id) {
    const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(scene_id);
    return entry == nullptr ? std::string(scene_id) : std::string(entry->label);
}

void set_ui_interaction(GLFWwindow* window, bool enabled, double& cursor_x, double& cursor_y) {
    glfwSetInputMode(window, GLFW_CURSOR, enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &cursor_x, &cursor_y);
}

const char* quality_mode_label(rt::viewer::ViewerQualityMode mode) {
    switch (mode) {
    case rt::viewer::ViewerQualityMode::preview:
        return "preview";
    case rt::viewer::ViewerQualityMode::converge:
        return "converge";
    }
    return "unknown";
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string scene_name = "final_room";
    argparse::ArgumentParser program("render_realtime_viewer");
    program.add_argument("--scene")
        .help("startup realtime scene id")
        .default_value(scene_name)
        .store_into(scene_name);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        fmt::print(stderr, "{}\n\n", err.what());
        fmt::print(stderr, "{}\n", fmt::streamed(program));
        return 1;
    }

    if (!is_supported_realtime_scene(scene_name)) {
        fmt::print(stderr, "--scene must reference a registered realtime scene\n");
        return 1;
    }

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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& imgui_io = ImGui::GetIO();
    (void)imgui_io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 120");

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

    rt::viewer::SceneSwitchController scene_controller(scene_name);
    rt::viewer::BodyPose pose = rt::default_spawn_pose_for_scene(scene_name);
    rt::PackedScene scene = rt::make_realtime_scene(scene_name).pack();
    rt::viewer::ViewerQualityController quality_controller(
        rt::viewer::default_viewer_preview_profile(),
        rt::viewer::default_viewer_converge_profile());
    std::string viewer_error_message;
    bool ui_interaction_enabled = false;
    bool previous_tab_down = false;

    rt::RendererPool pool(4);
    pool.prepare_scene(scene);

    double last_time = glfwGetTime();
    double last_cursor_x = 0.0;
    double last_cursor_y = 0.0;
    set_ui_interaction(window, ui_interaction_enabled, last_cursor_x, last_cursor_y);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        const bool tab_down = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tab_down && !previous_tab_down) {
            ui_interaction_enabled = !ui_interaction_enabled;
            set_ui_interaction(window, ui_interaction_enabled, last_cursor_x, last_cursor_y);
        }
        previous_tab_down = tab_down;

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
        const rt::viewer::ViewerFrameConvention frame_convention =
            rt::viewer_frame_convention_for_scene(scene_controller.current_scene_id());

        if (!ui_interaction_enabled) {
            rt::viewer::integrate_mouse_look(pose, delta_x, delta_y, kLookDegreesPerPixel);
            rt::viewer::integrate_wasd(pose, glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
                glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
                glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
                glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS,
                glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS,
                glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS,
                kMoveSpeedUnitsPerSecond * dt,
                frame_convention);
        }

        const std::string previous_scene_id = scene_controller.current_scene_id();
        const rt::viewer::SceneSwitchResult switch_result = scene_controller.resolve_pending();
        if (switch_result.applied) {
            try {
                rt::PackedScene next_scene = rt::make_realtime_scene(scene_controller.current_scene_id()).pack();
                pool.prepare_scene(next_scene);
                scene = std::move(next_scene);
                pose = rt::default_spawn_pose_for_scene(scene_controller.current_scene_id());
                quality_controller.reset_all();
                viewer_error_message.clear();
                set_ui_interaction(window, ui_interaction_enabled, last_cursor_x, last_cursor_y);
            } catch (const std::exception& err) {
                scene_controller.request_scene(previous_scene_id);
                (void)scene_controller.resolve_pending();
                viewer_error_message = err.what();
                fmt::print(stderr, "scene switch failed: {}\n", viewer_error_message);
            }
        } else if (!switch_result.error_message.empty()) {
            viewer_error_message = switch_result.error_message;
            fmt::print(stderr, "scene switch failed: {}\n", viewer_error_message);
        }

        quality_controller.begin_frame(scene_controller.current_scene_id(), pose);
        const rt::PackedCameraRig rig =
            rt::viewer::make_default_viewer_rig(pose, kViewWidth, kViewHeight, frame_convention).pack();
        const std::vector<rt::CameraRenderResult> results =
            pool.render_frame(rig, quality_controller.active_profile(), static_cast<int>(textures.size()));
        for (const auto& result : results) {
            const int idx = result.camera_index;
            if (idx < 0 || idx >= static_cast<int>(textures.size())) {
                continue;
            }
            const rt::viewer::ResolvedBeautyFrameView display_frame =
                quality_controller.resolve_beauty_view(idx, result.profiled.frame);
            const UploadStatus upload_status = upload_texture(
                textures[static_cast<std::size_t>(idx)],
                display_frame,
                texture_scratch[static_cast<std::size_t>(idx)]);
            if (upload_status == UploadStatus::ok) {
                warned_texture_mismatch[static_cast<std::size_t>(idx)] = false;
                continue;
            }

            if (!warned_texture_mismatch[static_cast<std::size_t>(idx)]) {
                if (upload_status == UploadStatus::resolution_mismatch) {
                    fmt::print(stderr,
                        "viewer frame size mismatch for camera {}: got {}x{}, expected {}x{}\n",
                        idx, display_frame.width, display_frame.height, kViewWidth, kViewHeight);
                } else {
                    fmt::print(stderr,
                        "viewer frame buffer too small for camera {}: got {}, need at least {}\n",
                        idx, display_frame.beauty_rgba.size(),
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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Viewer");
        ImGui::Text("Scene: %s", scene_label(scene_controller.current_scene_id()).c_str());
        ImGui::Text("Quality: %s", quality_mode_label(quality_controller.active_mode()));
        ImGui::Text("cam0 history: %d", quality_controller.history_length(0));
        ImGui::Text("Tab toggles UI cursor");
        ImGui::Text("Controls: WASD + QE move, mouse look");
        const std::string preview_label = scene_label(scene_controller.current_scene_id());
        if (ImGui::BeginCombo("Switch Scene", preview_label.c_str())) {
            for (const rt::SceneCatalogEntry& entry : rt::scene_catalog()) {
                const bool supported = entry.supports_realtime;
                const bool selected = entry.id == scene_controller.current_scene_id();
                if (!supported) {
                    ImGui::BeginDisabled();
                }
                const std::string label = std::string(entry.label);
                if (ImGui::Selectable(label.c_str(), selected) && supported) {
                    scene_controller.request_scene(std::string(entry.id));
                }
                if (!supported) {
                    ImGui::EndDisabled();
                }
            }
            ImGui::EndCombo();
        }
        if (!scene_controller.last_error().empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", scene_controller.last_error().c_str());
        } else if (!viewer_error_message.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", viewer_error_message.c_str());
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (label_list_base != 0) {
        glDeleteLists(label_list_base, static_cast<GLsizei>(kCameraLabels.size()));
    }
    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
