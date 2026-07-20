#include "scene/materialx_openpbr_loader.h"
#include "scene/openpbr_core_adapter.h"
#include "test_support.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    expect_true(argc == 2, "usage: test_public_openpbr_materials OPENPBR_EXAMPLES_DIR");
    const std::filesystem::path examples = argv[1];
    const auto documents = rt::scene::load_materialx_openpbr_directory(examples);
    expect_true(documents.size() == 83, "all pinned OpenPBR examples parsed");

    std::size_t compiled_count = 0;
    for (const auto& document : documents) {
        expect_true(document.document_color_space == "acescg",
            "official OpenPBR example declares ACEScg");
        static_cast<void>(rt::scene::compile_openpbr_core_material(document.surface));
        ++compiled_count;
    }
    std::cout << "compiled_openpbr_examples=" << compiled_count << '\n';
    return 0;
}
