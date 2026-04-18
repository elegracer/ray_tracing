#include "core/version.h"

#include "core/offline_shared_scene_renderer.h"
#include "realtime/scene_catalog.h"

#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <opencv2/opencv.hpp>

#include <cstdlib>
#include <exception>
#include <string>

namespace {

bool is_supported_cpu_scene(const std::string& scene_name) {
    const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(scene_name);
    return entry != nullptr && entry->supports_cpu_render;
}

}  // namespace

int main(int argc, const char* argv[]) {
    const std::string version_string = fmt::format("{}.{}.{}.{}", CORE_MAJOR_VERSION,
        CORE_MINOR_VERSION, CORE_PATCH_VERSION, CORE_TWEAK_VERSION);

    std::string output_image_format = "png";
    std::string scene_to_render = "cornell_box";

    argparse::ArgumentParser program("use_core", version_string);
    program.add_argument("--output_image_format")
        .help("Output image format, e.g. jpg, png, bmp")
        .default_value(output_image_format)
        .store_into(output_image_format);
    program.add_argument("--scene")
        .help("Registered offline scene id")
        .default_value(scene_to_render)
        .store_into(scene_to_render);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        fmt::print(stderr, "{}\n\n", err.what());
        fmt::print(stderr, "{}\n", fmt::streamed(program));
        return EXIT_FAILURE;
    }

    if (!is_supported_cpu_scene(scene_to_render)) {
        fmt::print(stderr, "--scene must reference a registered offline scene\n");
        return EXIT_FAILURE;
    }

    fmt::print("scene to render: {}\n", scene_to_render);
    fmt::print("output_image_format: {}\n", output_image_format);

    const cv::Mat image = rt::render_shared_scene(scene_to_render, 0);
    const std::string output_path = fmt::format("{}.{}", scene_to_render, output_image_format);
    if (!cv::imwrite(output_path, image)) {
        fmt::print(stderr, "failed to write {}\n", output_path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
