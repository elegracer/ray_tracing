#include "scene/openusd_stage_importer.h"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: test_public_usd_scene_import <stage.usd>\n";
        return EXIT_FAILURE;
    }
    if (!rt::scene::openusd_stage_importer_available()) {
        std::cerr << "public USD acceptance requires RT_ENABLE_OPENUSD=ON\n";
        return EXIT_FAILURE;
    }

    try {
        const rt::scene::SceneIRv2 scene =
            rt::scene::import_openusd_stage(std::filesystem::path {argv[1]});
        std::cout << "imported public USD acceptance stage with " << scene.prims().size()
                  << " SceneIR v2 prims\n";
    } catch (const std::exception& error) {
        std::cerr << "public USD acceptance import failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
