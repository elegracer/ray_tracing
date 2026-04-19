# External Scene Description Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace hard-coded C++ scene definitions with editable YAML scene files, add reload/rescan support in the viewer, and keep CPU and realtime scene semantics aligned while leaving room for future light-registry and MIS work.

**Architecture:** Add a file-backed `SceneDefinition` layer above the existing shared `SceneIR`, then route CPU presets, realtime presets, and scene catalog lookup through that layer. Keep CPU and realtime adapters conceptually unchanged by making them consume the same `SceneIR`, but extend the IR to support triangle meshes so YAML `obj_mtl` imports can feed both renderers.

**Tech Stack:** C++23, CMake, Eigen, yaml-cpp, tinyobjloader, ImGui/GLFW, existing CPU/realtime adapters and tests.

---

## File Structure

### New files

- `src/scene/scene_definition.h`
  File-backed scene metadata, presets, dependency tracking, and the shared `SceneIR`.
- `src/scene/scene_definition.cpp`
  Small helpers for scene-definition defaults and dependency normalization.
- `src/scene/yaml_scene_loader.h`
  Public YAML loader entrypoints and error-reporting types.
- `src/scene/yaml_scene_loader.cpp`
  YAML parsing, schema validation, include resolution, and asset-path rebasing.
- `src/scene/obj_mtl_importer.h`
  Public `OBJ + MTL` importer interface used by the YAML loader.
- `src/scene/obj_mtl_importer.cpp`
  Mesh parsing and material conversion into `SceneIR`.
- `src/scene/scene_file_catalog.h`
  Mutable file-backed catalog, dependency timestamps, reload/rescan API.
- `src/scene/scene_file_catalog.cpp`
  Catalog scan, scene load, reload, error retention, and atomic replacement logic.
- `src/common/triangle.h`
  CPU triangle hittable used by mesh-backed shared scenes.
- `tests/test_scene_definition.cpp`
  Unit tests for scene-definition defaults and dependency bookkeeping.
- `tests/test_yaml_scene_loader.cpp`
  YAML schema, include, and path-resolution tests.
- `tests/test_obj_mtl_importer.cpp`
  Importer conversion tests for geometry and basic MTL mapping.
- `tests/test_scene_file_catalog.cpp`
  Catalog scan/reload/rescan behavior tests.
- `tests/test_viewer_scene_reload.cpp`
  Viewer-facing reload/rescan behavior without the full GUI loop.
- `assets/scenes/common/materials/common_materials.yaml`
  Shared material fragment used by migrated scenes.
- `assets/scenes/final_room/scene.yaml`
  File-backed version of the existing `final_room`.
- `assets/scenes/cornell_box/scene.yaml`
  File-backed version of the existing `cornell_box`.
- `assets/scenes/simple_light/scene.yaml`
  File-backed version of the existing `simple_light`.
- `assets/scenes/imported_obj_smoke/scene.yaml`
  Minimal OBJ+MTL smoke-test scene.
- `assets/scenes/imported_obj_smoke/models/triangle.obj`
  Tiny mesh fixture for importer coverage.
- `assets/scenes/imported_obj_smoke/models/triangle.mtl`
  Tiny material fixture for importer coverage.

### Modified files

- `CMakeLists.txt`
  Add YAML/OBJ dependencies, loader/importer sources, and new tests.
- `src/scene/shared_scene_ir.h`
  Add triangle-mesh primitives and optional light-registry metadata hooks.
- `src/scene/cpu_scene_adapter.cpp`
  Adapt triangle meshes to CPU hittables.
- `src/scene/realtime_scene_adapter.cpp`
  Adapt triangle meshes to realtime scene description.
- `src/realtime/gpu/launch_params.h`
  Add packed triangle buffers to the realtime GPU scene view.
- `src/realtime/gpu/optix_renderer.h`
  Track uploaded triangle buffers and counts.
- `src/realtime/gpu/optix_renderer.cpp`
  Upload packed triangles and thread them through launch params.
- `src/realtime/gpu/programs.cu`
  Intersect triangles in the CUDA path so imported meshes render in realtime.
- `src/realtime/scene_description.h`
  Add triangle primitive storage in realtime scene description if absent.
- `src/realtime/scene_description.cpp`
  Implement triangle primitive packing and counts.
- `tests/test_scene_description.cpp`
  Validate triangle primitives survive packing.
- `src/realtime/scene_catalog.cpp`
  Replace static hard-coded metadata lookup with file-backed catalog data.
- `src/realtime/realtime_scene_factory.cpp`
  Load file-backed scene definitions, presets, and backgrounds.
- `src/realtime/viewer/scene_switch_controller.cpp`
  Validate scene availability against the mutable file-backed catalog.
- `utils/render_realtime_viewer.cpp`
  Add reload/rescan controls, automatic dirty-scene reload, and enlarge scene-selector UI.
- `src/core/offline_shared_scene_renderer.cpp`
  Resolve CPU presets from file-backed scene definitions.
- `src/scene/shared_scene_builders.h`
  Convert public lookup API from hard-coded builders to file-backed definitions while keeping call sites stable during migration.
- `src/scene/shared_scene_builders.cpp`
  Provide compatibility shims over the new catalog-backed scene-definition source.
- `tests/test_shared_scene_builders.cpp`
  Update expectations so the existing public API is validated against file-backed scenes.
- `tests/test_cpu_scene_adapter.cpp`
  Add mesh coverage once `SceneIR` can express triangles.
- `tests/test_realtime_scene_adapter.cpp`
  Add mesh coverage once realtime packing supports triangles.
- `tests/test_realtime_scene_factory.cpp`
  Validate realtime presets now come from YAML scenes.
- `tests/test_scene_catalog.cpp`
  Validate the public catalog now reflects file-backed discovery.

## Task 1: Add SceneDefinition and build-system support

**Files:**
- Create: `src/scene/scene_definition.h`
- Create: `src/scene/scene_definition.cpp`
- Create: `tests/test_scene_definition.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing scene-definition test**

```cpp
#include "scene/scene_definition.h"

#include <stdexcept>

namespace {

void expect_true(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    rt::scene::SceneDefinition scene;
    scene.metadata.id = "demo";
    scene.metadata.label = "Demo";
    scene.metadata.supports_cpu_render = true;
    scene.metadata.supports_realtime = true;
    scene.dependencies.push_back("assets/scenes/demo/scene.yaml");

    expect_true(scene.metadata.id == "demo", "scene id");
    expect_true(scene.cpu_presets.empty(), "cpu presets start empty");
    expect_true(scene.realtime_preset.has_value() == false, "no realtime preset by default");
    expect_true(scene.dependencies.size() == 1, "dependency recorded");
    return 0;
}
```

- [ ] **Step 2: Register the new sources and test in CMake**

```cmake
target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_definition.cpp
)

add_executable(test_scene_definition)
target_sources(test_scene_definition
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene_definition.cpp
)
target_link_libraries(test_scene_definition PRIVATE core)
add_test(NAME test_scene_definition COMMAND test_scene_definition)
```

- [ ] **Step 3: Run the new test to verify the type does not exist yet**

Run: `cmake --build build --target test_scene_definition -j4`

Expected: FAIL with a compiler error mentioning `scene/scene_definition.h` or `rt::scene::SceneDefinition` not found.

- [ ] **Step 4: Add the minimal SceneDefinition type**

```cpp
#pragma once

#include "scene/shared_scene_builders.h"
#include "scene/shared_scene_ir.h"

#include <optional>
#include <string>
#include <vector>

namespace rt::scene {

struct SceneDefinition {
    SceneMetadata metadata {};
    SceneIR scene_ir {};
    std::vector<CpuRenderPreset> cpu_presets;
    std::optional<RealtimeViewPreset> realtime_preset;
    std::vector<std::string> dependencies;
};

}  // namespace rt::scene
```

```cpp
#include "scene/scene_definition.h"

namespace rt::scene {}  // namespace rt::scene
```

- [ ] **Step 5: Run the test to verify the new type builds**

Run: `cmake --build build --target test_scene_definition -j4 && ctest --test-dir build --output-on-failure -R test_scene_definition`

Expected:

```text
100% tests passed, 0 tests failed out of 1
```

- [ ] **Step 6: Commit the bootstrap**

```bash
git add CMakeLists.txt \
  src/scene/scene_definition.h src/scene/scene_definition.cpp \
  tests/test_scene_definition.cpp
git commit -m "build: add scene definition scaffolding"
```

## Task 2: Extend SceneIR and adapters for triangle meshes

**Files:**
- Modify: `src/scene/shared_scene_ir.h`
- Create: `src/common/triangle.h`
- Modify: `src/scene/cpu_scene_adapter.cpp`
- Modify: `src/scene/realtime_scene_adapter.cpp`
- Modify: `src/realtime/scene_description.h`
- Modify: `src/realtime/scene_description.cpp`
- Modify: `src/realtime/gpu/launch_params.h`
- Modify: `src/realtime/gpu/optix_renderer.h`
- Modify: `src/realtime/gpu/optix_renderer.cpp`
- Modify: `src/realtime/gpu/programs.cu`
- Modify: `tests/test_cpu_scene_adapter.cpp`
- Modify: `tests/test_realtime_scene_adapter.cpp`
- Modify: `tests/test_scene_description.cpp`

- [ ] **Step 1: Add failing CPU and realtime mesh-adapter tests**

```cpp
void test_cpu_adapter_accepts_triangle_mesh() {
    rt::scene::SceneIR scene;
    const int white = scene.add_texture(rt::scene::ConstantColorTextureDesc {.color = Eigen::Vector3d::Ones()});
    const int matte = scene.add_material(rt::scene::DiffuseMaterial {.albedo_texture = white});
    const int mesh = scene.add_shape(rt::scene::TriangleMeshShape {
        .positions = {
            Eigen::Vector3d {0.0, 0.0, 0.0},
            Eigen::Vector3d {1.0, 0.0, 0.0},
            Eigen::Vector3d {0.0, 1.0, 0.0},
        },
        .normals = {},
        .uvs = {},
        .triangles = {Eigen::Vector3i {0, 1, 2}},
    });
    scene.add_instance(rt::scene::SurfaceInstance {.shape_index = mesh, .material_index = matte});

    const auto adapted = rt::scene::adapt_to_cpu(scene);
    expect_true(static_cast<bool>(adapted.world), "triangle mesh world");
}

void test_realtime_adapter_emits_triangle_primitives() {
    rt::scene::SceneIR scene;
    const int white = scene.add_texture(rt::scene::ConstantColorTextureDesc {.color = Eigen::Vector3d::Ones()});
    const int matte = scene.add_material(rt::scene::DiffuseMaterial {.albedo_texture = white});
    const int mesh = scene.add_shape(rt::scene::TriangleMeshShape {
        .positions = {
            Eigen::Vector3d {0.0, 0.0, 0.0},
            Eigen::Vector3d {1.0, 0.0, 0.0},
            Eigen::Vector3d {0.0, 1.0, 0.0},
        },
        .normals = {},
        .uvs = {},
        .triangles = {Eigen::Vector3i {0, 1, 2}},
    });
    scene.add_instance(rt::scene::SurfaceInstance {.shape_index = mesh, .material_index = matte});

    const rt::SceneDescription realtime = rt::scene::adapt_to_realtime(scene);
    expect_true(realtime.triangles().size() == 1, "triangle primitive count");
}
```

- [ ] **Step 2: Run the mesh tests and verify they fail**

Run: `cmake --build build --target test_cpu_scene_adapter test_realtime_scene_adapter -j4 && ctest --test-dir build --output-on-failure -R "test_cpu_scene_adapter|test_realtime_scene_adapter"`

Expected: FAIL with compiler errors mentioning `TriangleMeshShape` or `triangles()` missing.

- [ ] **Step 3: Add mesh support to the shared IR and realtime scene description**

```cpp
struct TriangleMeshShape {
    std::vector<Eigen::Vector3d> positions;
    std::vector<Eigen::Vector3d> normals;
    std::vector<Eigen::Vector2d> uvs;
    std::vector<Eigen::Vector3i> triangles;
};

using ShapeDesc = std::variant<SphereShape, QuadShape, BoxShape, TriangleMeshShape>;
```

```cpp
struct TrianglePrimitive {
    int material_index = -1;
    Eigen::Vector3d p0 = Eigen::Vector3d::Zero();
    Eigen::Vector3d p1 = Eigen::Vector3d::Zero();
    Eigen::Vector3d p2 = Eigen::Vector3d::Zero();
    bool dynamic = false;
};

class SceneDescription {
   public:
    void add_triangle(const TrianglePrimitive& triangle);
    const std::vector<TrianglePrimitive>& triangles() const;
};
```

- [ ] **Step 4: Teach both adapters to expand triangle meshes**

```cpp
// src/common/triangle.h
struct Triangle {
    Triangle(const Vec3d& a, const Vec3d& b, const Vec3d& c, pro::proxy<Material> mat)
        : m_a(a), m_b(b), m_c(c), m_mat(mat), m_bbox(AABB {a, b}, AABB {a, c}) {}

    AABB bounding_box() const { return m_bbox; }

    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const;

    Vec3d m_a;
    Vec3d m_b;
    Vec3d m_c;
    pro::proxy<Material> m_mat;
    AABB m_bbox;
};
```

```cpp
// cpu_scene_adapter.cpp
if constexpr (std::is_same_v<T, TriangleMeshShape>) {
    HittableList mesh_world;
    for (const Eigen::Vector3i& tri : desc.triangles) {
        mesh_world.add(pro::make_proxy_shared<Hittable, Triangle>(
            to_vec3(desc.positions[tri.x()]),
            to_vec3(desc.positions[tri.y()]),
            to_vec3(desc.positions[tri.z()]),
            material));
    }
    return pro::make_proxy_shared<Hittable, HittableList>(mesh_world);
}
```

```cpp
// realtime_scene_adapter.cpp
if constexpr (std::is_same_v<T, TriangleMeshShape>) {
    for (const Eigen::Vector3i& tri : desc.triangles) {
        out.add_triangle(TrianglePrimitive {
            .material_index = instance.material_index,
            .p0 = transform_point(instance.transform, desc.positions[tri.x()]),
            .p1 = transform_point(instance.transform, desc.positions[tri.y()]),
            .p2 = transform_point(instance.transform, desc.positions[tri.z()]),
            .dynamic = false,
        });
    }
}
```

```cpp
// realtime/gpu/launch_params.h + programs.cu
struct PackedTriangle {
    Eigen::Vector3f p0;
    float pad0 = 0.0f;
    Eigen::Vector3f p1;
    float pad1 = 0.0f;
    Eigen::Vector3f p2;
    int material_index = -1;
};

for (int i = 0; i < scene.triangle_count; ++i) {
    const PackedTriangle& tri = scene.triangles[i];
    if (intersect_triangle(ray, tri, hit)) {
        best_hit = hit;
    }
}
```

- [ ] **Step 5: Run the adapter tests**

Run: `cmake --build build --target test_cpu_scene_adapter test_realtime_scene_adapter -j4 && ctest --test-dir build --output-on-failure -R "test_cpu_scene_adapter|test_realtime_scene_adapter"`

Expected:

```text
2/2 tests passed
```

- [ ] **Step 6: Run triangle-packing coverage**

Run: `cmake --build build --target test_scene_description -j4 && ctest --test-dir build --output-on-failure -R test_scene_description`

Expected:

```text
100% tests passed, 0 tests failed out of 1
```

- [ ] **Step 7: Commit mesh support**

```bash
git add src/common/triangle.h \
  src/scene/shared_scene_ir.h \
  src/scene/cpu_scene_adapter.cpp src/scene/realtime_scene_adapter.cpp \
  src/realtime/scene_description.h src/realtime/scene_description.cpp \
  src/realtime/gpu/launch_params.h src/realtime/gpu/optix_renderer.h \
  src/realtime/gpu/optix_renderer.cpp src/realtime/gpu/programs.cu \
  tests/test_cpu_scene_adapter.cpp tests/test_realtime_scene_adapter.cpp \
  tests/test_scene_description.cpp
git commit -m "feat: add triangle mesh support to shared scene ir"
```

## Task 3: Implement YAML scene loading for core scene data and presets

**Files:**
- Create: `src/scene/yaml_scene_loader.h`
- Create: `src/scene/yaml_scene_loader.cpp`
- Modify: `src/scene/scene_definition.h`
- Modify: `tests/test_yaml_scene_loader.cpp`

- [ ] **Step 1: Write a failing YAML loader test for embedded scene data**

```cpp
#include "scene/yaml_scene_loader.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

int main() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_basic";
    fs::create_directories(root);
    const fs::path scene_file = root / "scene.yaml";
    std::ofstream(scene_file) << R"(format_version: 1
scene:
  id: basic_room
  label: Basic Room
  background: [0.1, 0.2, 0.3]
  textures:
    white:
      type: constant
      color: [1.0, 1.0, 1.0]
  materials:
    matte:
      type: diffuse
      albedo: white
  shapes:
    ball:
      type: sphere
      center: [0.0, 0.0, 0.0]
      radius: 1.0
  instances:
    - shape: ball
      material: matte
cpu_presets:
  default:
    samples_per_pixel: 64
    camera:
      lookfrom: [0.0, 0.0, 5.0]
      lookat: [0.0, 0.0, 0.0]
      vfov: 40.0
      aspect_ratio: 1.7777778
      image_width: 640
      max_depth: 16
      vup: [0.0, 1.0, 0.0]
      defocus_angle: 0.0
      focus_dist: 5.0
realtime:
  default_view:
    initial_body_pose:
      position: [0.0, 0.0, 2.0]
      yaw_deg: 0.0
      pitch_deg: 0.0
    frame_convention: world_y_up
    vfov_deg: 67.0
    use_default_viewer_intrinsics: false
    base_move_speed: 2.5
)";

    const rt::scene::SceneDefinition loaded = rt::scene::load_scene_definition(scene_file);
    expect_true(loaded.metadata.id == "basic_room", "scene id");
    expect_true(loaded.metadata.background.isApprox(Eigen::Vector3d(0.1, 0.2, 0.3)), "background");
    expect_true(loaded.cpu_presets.size() == 1, "cpu preset count");
    expect_true(loaded.realtime_preset.has_value(), "realtime preset");
    expect_true(loaded.scene_ir.surface_instances().size() == 1, "instance count");
    return 0;
}
```

- [ ] **Step 2: Run the YAML loader test and verify it fails**

Run: `cmake --build build --target test_yaml_scene_loader -j4`

Expected: FAIL with compiler errors for `load_scene_definition`.

- [ ] **Step 3: Add the loader interface and minimal schema parsing**

```cpp
#pragma once

#include "scene/scene_definition.h"

#include <filesystem>

namespace rt::scene {

SceneDefinition load_scene_definition(const std::filesystem::path& scene_file);

}  // namespace rt::scene
```

```cpp
SceneDefinition load_scene_definition(const std::filesystem::path& scene_file) {
    const YAML::Node root = YAML::LoadFile(scene_file.string());
    SceneDefinition out;
    out.metadata.id = root["scene"]["id"].as<std::string>();
    out.metadata.label = root["scene"]["label"].as<std::string>();
    out.metadata.background = parse_vec3(root["scene"]["background"]);
    out.metadata.supports_cpu_render = root["cpu_presets"] && root["cpu_presets"]["default"];
    out.metadata.supports_realtime = root["realtime"] && root["realtime"]["default_view"];
    out.dependencies.push_back(scene_file.string());

    parse_textures(root["scene"]["textures"], out.scene_ir);
    parse_materials(root["scene"]["materials"], out.scene_ir);
    parse_shapes(root["scene"]["shapes"], out.scene_ir, shape_ids);
    parse_instances(root["scene"]["instances"], out.scene_ir, shape_ids, material_ids);
    parse_cpu_presets(root["cpu_presets"], out.cpu_presets);
    out.realtime_preset = parse_realtime_preset(root["realtime"]["default_view"]);
    return out;
}
```

```cmake
find_package(yaml-cpp CONFIG REQUIRED)

target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/yaml_scene_loader.cpp
)
target_link_libraries(core
    PUBLIC
        yaml-cpp::yaml-cpp
)
```

- [ ] **Step 4: Run the YAML loader test**

Run: `cmake --build build --target test_yaml_scene_loader -j4 && ctest --test-dir build --output-on-failure -R test_yaml_scene_loader`

Expected:

```text
100% tests passed, 0 tests failed out of 1
```

- [ ] **Step 5: Commit the core loader**

```bash
git add src/scene/scene_definition.h src/scene/yaml_scene_loader.h src/scene/yaml_scene_loader.cpp \
  tests/test_yaml_scene_loader.cpp
git commit -m "feat: load core scene definitions from yaml"
```

## Task 4: Add include resolution, path rebasing, and OBJ+MTL import

**Files:**
- Create: `src/scene/obj_mtl_importer.h`
- Create: `src/scene/obj_mtl_importer.cpp`
- Modify: `src/scene/yaml_scene_loader.cpp`
- Modify: `tests/test_yaml_scene_loader.cpp`
- Create: `tests/test_obj_mtl_importer.cpp`
- Create: `assets/scenes/imported_obj_smoke/scene.yaml`
- Create: `assets/scenes/imported_obj_smoke/models/triangle.obj`
- Create: `assets/scenes/imported_obj_smoke/models/triangle.mtl`

- [ ] **Step 1: Write failing tests for includes and OBJ import**

```cpp
void test_yaml_loader_resolves_includes_and_relative_paths() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_include";
    fs::create_directories(root / "common");
    std::ofstream(root / "common" / "materials.yaml") << R"(scene:
  textures:
    white:
      type: constant
      color: [1.0, 1.0, 1.0]
  materials:
    matte:
      type: diffuse
      albedo: white
)";
    std::ofstream(root / "scene.yaml") << R"(format_version: 1
includes:
  - common/materials.yaml
scene:
  id: include_room
  label: Include Room
  background: [0.0, 0.0, 0.0]
  shapes:
    ball:
      type: sphere
      center: [0.0, 0.0, 0.0]
      radius: 1.0
  instances:
    - shape: ball
      material: matte
)";

    const rt::scene::SceneDefinition loaded = rt::scene::load_scene_definition(root / "scene.yaml");
    expect_true(loaded.scene_ir.materials().size() == 1, "included material");
    expect_true(loaded.dependencies.size() == 2, "include dependency tracked");
}

void test_obj_mtl_importer_creates_triangle_mesh_shape() {
    const fs::path root = fs::temp_directory_path() / "obj_mtl_importer";
    fs::create_directories(root);
    std::ofstream(root / "triangle.obj") << "mtllib triangle.mtl\nusemtl matte\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    std::ofstream(root / "triangle.mtl") << "newmtl matte\nKd 0.8 0.7 0.6\n";

    const rt::scene::ObjImportResult imported = rt::scene::import_obj_mtl(root / "triangle.obj");
    expect_true(imported.scene_ir.shapes().size() == 1, "mesh shape count");
    expect_true(std::holds_alternative<rt::scene::TriangleMeshShape>(imported.scene_ir.shapes().front()), "mesh type");
    expect_true(imported.scene_ir.materials().size() == 1, "material count");
}
```

- [ ] **Step 2: Run the include/import tests and verify they fail**

Run: `cmake --build build --target test_yaml_scene_loader test_obj_mtl_importer -j4 && ctest --test-dir build --output-on-failure -R "test_yaml_scene_loader|test_obj_mtl_importer"`

Expected: FAIL with compiler errors for `import_obj_mtl` and missing include behavior.

- [ ] **Step 3: Implement the importer interface**

```cpp
#pragma once

#include "scene/shared_scene_ir.h"

#include <filesystem>
#include <string>
#include <vector>

namespace rt::scene {

struct ObjImportResult {
    SceneIR scene_ir {};
    std::vector<std::string> dependencies;
};

ObjImportResult import_obj_mtl(const std::filesystem::path& obj_file);

}  // namespace rt::scene
```

```cpp
ObjImportResult import_obj_mtl(const std::filesystem::path& obj_file) {
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(obj_file.string())) {
        throw std::runtime_error(reader.Error());
    }

    ObjImportResult out;
    out.dependencies.push_back(obj_file.string());
    if (!reader.GetAttrib().vertices.empty()) {
        TriangleMeshShape mesh;
        const tinyobj::attrib_t& attrib = reader.GetAttrib();
        for (std::size_t i = 0; i < attrib.vertices.size(); i += 3) {
            mesh.positions.push_back(Eigen::Vector3d {
                attrib.vertices[i + 0],
                attrib.vertices[i + 1],
                attrib.vertices[i + 2],
            });
        }
        const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
        for (const tinyobj::shape_t& shape : shapes) {
            for (std::size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
                mesh.triangles.push_back(Eigen::Vector3i {
                    shape.mesh.indices[i + 0].vertex_index,
                    shape.mesh.indices[i + 1].vertex_index,
                    shape.mesh.indices[i + 2].vertex_index,
                });
            }
        }
        out.scene_ir.add_shape(mesh);
    }
    return out;
}
```

```cmake
find_package(tinyobjloader CONFIG REQUIRED)

target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/obj_mtl_importer.cpp
)
target_link_libraries(core
    PUBLIC
        tinyobjloader::tinyobjloader
)
```

- [ ] **Step 4: Extend YAML loading to merge includes and imports**

```cpp
for (const YAML::Node& include_node : root["includes"]) {
    const std::filesystem::path include_path = scene_file.parent_path() / include_node.as<std::string>();
    const SceneDefinition included = load_scene_definition(include_path);
    append_unique(out.dependencies, included.dependencies);
    merge_scene_definition(included, out);
}

for (const auto& [import_id, import_node] : root["imports"]) {
    if (import_node["type"].as<std::string>() != "obj_mtl") {
        throw std::invalid_argument("unsupported import type");
    }
    const ObjImportResult imported = import_obj_mtl(scene_file.parent_path() / import_node["obj"].as<std::string>());
    merge_import(import_id.as<std::string>(), imported, out);
}
```

- [ ] **Step 5: Run the loader and importer tests**

Run: `cmake --build build --target test_yaml_scene_loader test_obj_mtl_importer -j4 && ctest --test-dir build --output-on-failure -R "test_yaml_scene_loader|test_obj_mtl_importer"`

Expected:

```text
2/2 tests passed
```

- [ ] **Step 6: Commit include/import support**

```bash
git add src/scene/obj_mtl_importer.h src/scene/obj_mtl_importer.cpp \
  src/scene/yaml_scene_loader.cpp tests/test_yaml_scene_loader.cpp \
  tests/test_obj_mtl_importer.cpp \
  assets/scenes/imported_obj_smoke/scene.yaml \
  assets/scenes/imported_obj_smoke/models/triangle.obj \
  assets/scenes/imported_obj_smoke/models/triangle.mtl
git commit -m "feat: add yaml includes and obj scene imports"
```

## Task 5: Build the mutable scene catalog with reload and rescan

**Files:**
- Create: `src/scene/scene_file_catalog.h`
- Create: `src/scene/scene_file_catalog.cpp`
- Modify: `src/scene/shared_scene_builders.h`
- Modify: `src/scene/shared_scene_builders.cpp`
- Modify: `src/realtime/scene_catalog.cpp`
- Create: `tests/test_scene_file_catalog.cpp`
- Modify: `tests/test_scene_catalog.cpp`
- Modify: `tests/test_shared_scene_builders.cpp`

- [ ] **Step 1: Write failing catalog tests for scan/reload/rescan**

```cpp
#include "scene/scene_file_catalog.h"

int main() {
    rt::scene::SceneFileCatalog catalog;
    catalog.scan_directory("assets/scenes");

    const auto entries = catalog.entries();
    expect_true(entries.empty() == false, "catalog has scenes");
    expect_true(catalog.find_scene("final_room") != nullptr, "final_room loaded");

    const auto status = catalog.reload_scene("final_room");
    expect_true(status.ok, "reload current scene");
    return 0;
}
```

```cpp
void test_public_scene_catalog_uses_file_backed_entries() {
    const auto& entries = rt::scene_catalog();
    expect_true(entries.empty() == false, "scene catalog non-empty");
    expect_true(rt::find_scene_catalog_entry("final_room") != nullptr, "final_room visible");
}
```

- [ ] **Step 2: Run the catalog tests and verify they fail**

Run: `cmake --build build --target test_scene_file_catalog test_scene_catalog test_shared_scene_builders -j4 && ctest --test-dir build --output-on-failure -R "test_scene_file_catalog|test_scene_catalog|test_shared_scene_builders"`

Expected: FAIL with compiler errors for `SceneFileCatalog` and still-static hard-coded metadata.

- [ ] **Step 3: Add the catalog API**

```cpp
#pragma once

#include "scene/scene_definition.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rt::scene {

struct ReloadStatus {
    bool ok = false;
    std::string error_message;
};

class SceneFileCatalog {
   public:
    void scan_directory(const std::filesystem::path& root);
    ReloadStatus reload_scene(std::string_view scene_id);
    const SceneDefinition* find_scene(std::string_view scene_id) const;
    const std::vector<SceneMetadata>& entries() const;
};

SceneFileCatalog& global_scene_file_catalog();

}  // namespace rt::scene
```

```cmake
target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_file_catalog.cpp
)
```

- [ ] **Step 4: Route public scene lookups through the global file catalog**

```cpp
const std::vector<SceneMetadata>& scene_metadata() {
    return global_scene_file_catalog().entries();
}

const SceneDefinition* definition = global_scene_file_catalog().find_scene(scene_id);
if (definition == nullptr) {
    return nullptr;
}
return definition->cpu_presets.empty() ? nullptr : &definition->cpu_presets.front();
```

```cpp
const std::vector<SceneCatalogEntry>& scene_catalog() {
    static std::vector<SceneCatalogEntry> cache;
    cache.clear();
    for (const scene::SceneMetadata& metadata : scene::scene_metadata()) {
        cache.push_back(SceneCatalogEntry {
            .id = metadata.id,
            .label = metadata.label,
            .supports_cpu_render = metadata.supports_cpu_render,
            .supports_realtime = metadata.supports_realtime,
        });
    }
    return cache;
}
```

- [ ] **Step 5: Run the catalog and compatibility tests**

Run: `cmake --build build --target test_scene_file_catalog test_scene_catalog test_shared_scene_builders -j4 && ctest --test-dir build --output-on-failure -R "test_scene_file_catalog|test_scene_catalog|test_shared_scene_builders"`

Expected:

```text
3/3 tests passed
```

- [ ] **Step 6: Commit the file-backed catalog**

```bash
git add src/scene/scene_file_catalog.h src/scene/scene_file_catalog.cpp \
  src/scene/shared_scene_builders.h src/scene/shared_scene_builders.cpp \
  src/realtime/scene_catalog.cpp \
  tests/test_scene_file_catalog.cpp tests/test_scene_catalog.cpp tests/test_shared_scene_builders.cpp
git commit -m "feat: add file-backed scene catalog"
```

## Task 6: Move CPU and realtime scene construction to file-backed definitions

**Files:**
- Modify: `src/core/offline_shared_scene_renderer.cpp`
- Modify: `src/realtime/realtime_scene_factory.cpp`
- Modify: `src/realtime/viewer/scene_switch_controller.cpp`
- Modify: `tests/test_realtime_scene_factory.cpp`
- Modify: `tests/test_offline_shared_scene_renderer.cpp`

- [ ] **Step 1: Write failing integration tests for preset-backed scene loading**

```cpp
void test_offline_renderer_uses_file_backed_default_preset() {
    const auto result = rt::render_shared_scene_reference("final_room", "default");
    expect_true(result.width > 0, "cpu render width");
}

void test_realtime_factory_uses_file_backed_view_preset() {
    const rt::viewer::BodyPose pose = rt::default_spawn_pose_for_scene("final_room");
    expect_true(pose.position.norm() > 0.0, "spawn pose from yaml");
}
```

- [ ] **Step 2: Run the CPU/realtime factory tests and verify they fail**

Run: `cmake --build build --target test_offline_shared_scene_renderer test_realtime_scene_factory -j4 && ctest --test-dir build --output-on-failure -R "test_offline_shared_scene_renderer|test_realtime_scene_factory"`

Expected: FAIL because `build_scene()` and preset lookup still depend on hard-coded builders.

- [ ] **Step 3: Replace direct hard-coded builder calls with SceneDefinition access**

```cpp
const scene::SceneDefinition* definition = scene::global_scene_file_catalog().find_scene(scene_id);
if (definition == nullptr) {
    throw std::invalid_argument("unknown scene id");
}
const scene::CpuRenderPreset* preset = scene::find_cpu_render_preset(scene_id, preset_id);
const scene::CpuSceneAdapterResult adapted = scene::adapt_to_cpu(definition->scene_ir);
```

```cpp
SceneDescription make_realtime_scene(std::string_view scene_id) {
    const scene::SceneDefinition* definition = scene::global_scene_file_catalog().find_scene(scene_id);
    if (definition == nullptr || !definition->metadata.supports_realtime) {
        throw std::invalid_argument("unsupported realtime scene");
    }
    SceneDescription out = scene::adapt_to_realtime(definition->scene_ir);
    out.background = definition->metadata.background;
    return out;
}
```

- [ ] **Step 4: Run the CPU/realtime integration tests**

Run: `cmake --build build --target test_offline_shared_scene_renderer test_realtime_scene_factory -j4 && ctest --test-dir build --output-on-failure -R "test_offline_shared_scene_renderer|test_realtime_scene_factory"`

Expected:

```text
2/2 tests passed
```

- [ ] **Step 5: Commit the factory migration**

```bash
git add src/core/offline_shared_scene_renderer.cpp \
  src/realtime/realtime_scene_factory.cpp src/realtime/viewer/scene_switch_controller.cpp \
  tests/test_realtime_scene_factory.cpp tests/test_offline_shared_scene_renderer.cpp
git commit -m "refactor: load cpu and realtime scenes from file definitions"
```

## Task 7: Add viewer reload/rescan behavior and enlarge the scene selector

**Files:**
- Modify: `utils/render_realtime_viewer.cpp`
- Create: `tests/test_viewer_scene_reload.cpp`
- Modify: `tests/test_viewer_scene_switch_controller.cpp`

- [ ] **Step 1: Write failing tests for reload/rescan behavior**

```cpp
#include "scene/scene_file_catalog.h"
#include "realtime/viewer/scene_switch_controller.h"

int main() {
    rt::scene::SceneFileCatalog catalog;
    catalog.scan_directory("assets/scenes");

    rt::viewer::SceneSwitchController controller("final_room");
    controller.request_scene("cornell_box");
    const auto switched = controller.resolve_pending();
    expect_true(switched.applied, "switch applied");

    const auto reloaded = catalog.reload_scene("cornell_box");
    expect_true(reloaded.ok, "reload ok");
    return 0;
}
```

- [ ] **Step 2: Run the viewer reload tests and verify they fail**

Run: `cmake --build build --target test_viewer_scene_reload test_viewer_scene_switch_controller render_realtime_viewer -j4 && ctest --test-dir build --output-on-failure -R "test_viewer_scene_reload|test_viewer_scene_switch_controller"`

Expected: FAIL because the viewer has no reload/rescan glue and `SceneSwitchController` still assumes a static catalog.

- [ ] **Step 3: Add UI controls and dirty-scene reload**

```cpp
if (ImGui::Button("Reload Current Scene")) {
    const auto status = rt::scene::global_scene_file_catalog().reload_scene(scene_switch.current_scene_id());
    reload_error = status.error_message;
    if (status.ok) {
        request_scene_reload = true;
    }
}
ImGui::SameLine();
if (ImGui::Button("Rescan Scene Directory")) {
    rt::scene::global_scene_file_catalog().scan_directory("assets/scenes");
}

ImGui::SetNextItemWidth(400.0f);
if (ImGui::BeginCombo("Scene", current_label.c_str(), ImGuiComboFlags_HeightLargest)) {
    ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, 0.0f), ImVec2(400.0f, 360.0f));
    for (const rt::SceneCatalogEntry& entry : rt::scene_catalog()) {
        const bool selected = entry.id == scene_switch.current_scene_id();
        if (ImGui::Selectable(entry.label.c_str(), selected)) {
            scene_switch.request_scene(entry.id);
        }
        if (selected) {
            ImGui::SetItemDefaultFocus();
        }
    }
    ImGui::EndCombo();
}
```

- [ ] **Step 4: Rebuild the viewer and run the reload tests**

Run: `cmake --build build --target test_viewer_scene_reload test_viewer_scene_switch_controller render_realtime_viewer -j4 && ctest --test-dir build --output-on-failure -R "test_viewer_scene_reload|test_viewer_scene_switch_controller"`

Expected:

```text
2/2 tests passed
```

- [ ] **Step 5: Commit the viewer reload workflow**

```bash
git add utils/render_realtime_viewer.cpp \
  tests/test_viewer_scene_reload.cpp tests/test_viewer_scene_switch_controller.cpp
git commit -m "feat: add scene reload and rescan controls to viewer"
```

## Task 8: Migrate the shipped scenes into YAML assets and lock regression coverage

**Files:**
- Create: `assets/scenes/final_room/scene.yaml`
- Create: `assets/scenes/cornell_box/scene.yaml`
- Create: `assets/scenes/simple_light/scene.yaml`
- Create: `assets/scenes/common/materials/common_materials.yaml`
- Modify: `tests/test_shared_scene_regression.cpp`
- Modify: `tests/test_render_profile.cpp`
- Modify: `tests/test_viewer_quality_reference.cpp`
- Modify: `tests/test_realtime_scene_factory.cpp`

- [ ] **Step 1: Write failing regression tests against migrated file-backed scenes**

```cpp
void test_final_room_file_scene_matches_previous_shape_counts() {
    const rt::scene::SceneDefinition* definition =
        rt::scene::global_scene_file_catalog().find_scene("final_room");
    expect_true(definition != nullptr, "final_room definition");
    expect_true(definition->scene_ir.surface_instances().size() >= 6, "final_room surfaces");
    expect_true(definition->metadata.supports_realtime, "final_room realtime");
}

void test_simple_light_file_scene_keeps_cpu_default_preset() {
    const auto* preset = rt::scene::default_cpu_render_preset("simple_light");
    expect_true(preset != nullptr, "simple_light default preset");
    expect_true(preset->samples_per_pixel > 0, "simple_light spp");
}
```

- [ ] **Step 2: Run the regression tests and verify they fail before migration**

Run: `cmake --build build --target test_shared_scene_regression test_viewer_quality_reference test_realtime_scene_factory -j4 && ctest --test-dir build --output-on-failure -R "test_shared_scene_regression|test_viewer_quality_reference|test_realtime_scene_factory"`

Expected: FAIL because the YAML asset directories do not exist yet.

- [ ] **Step 3: Add the migrated scene YAML files**

```yaml
format_version: 1

includes:
  - ../common/materials/common_materials.yaml

scene:
  id: final_room
  label: Final Room
  background: [0.0, 0.0, 0.0]
  textures:
    ceiling_light_tex:
      type: constant
      color: [10.0, 10.0, 10.0]
  materials:
    ceiling_light:
      type: emissive
      emission: ceiling_light_tex
  shapes:
    ceiling_light_quad:
      type: quad
      origin: [-1.0, 3.15, -1.0]
      edge_u: [2.0, 0.0, 0.0]
      edge_v: [0.0, 0.0, 2.0]
  instances:
    - shape: ceiling_light_quad
      material: ceiling_light
cpu_presets:
  default:
    samples_per_pixel: 500
    camera:
      lookfrom: [13.0, 2.0, 3.0]
      lookat: [0.0, 0.0, 0.0]
      vfov: 20.0
      aspect_ratio: 1.7777778
      image_width: 1280
      max_depth: 50
      vup: [0.0, 1.0, 0.0]
      defocus_angle: 0.0
      focus_dist: 10.0
realtime:
  default_view:
    initial_body_pose:
      position: [0.0, 0.0, 1.8]
      yaw_deg: 0.0
      pitch_deg: 0.0
    frame_convention: world_z_up
    vfov_deg: 67.38
    use_default_viewer_intrinsics: true
    base_move_speed: 2.0
```

- [ ] **Step 4: Run the migrated-scene regression suite**

Run: `cmake --build build --target test_shared_scene_regression test_viewer_quality_reference test_realtime_scene_factory test_scene_catalog test_shared_scene_builders -j4 && ctest --test-dir build --output-on-failure -R "test_shared_scene_regression|test_viewer_quality_reference|test_realtime_scene_factory|test_scene_catalog|test_shared_scene_builders"`

Expected:

```text
5/5 tests passed
```

- [ ] **Step 5: Commit the file-backed scene migration**

```bash
git add assets/scenes/common/materials/common_materials.yaml \
  assets/scenes/final_room/scene.yaml assets/scenes/cornell_box/scene.yaml \
  assets/scenes/simple_light/scene.yaml \
  tests/test_shared_scene_regression.cpp tests/test_render_profile.cpp \
  tests/test_viewer_quality_reference.cpp tests/test_realtime_scene_factory.cpp
git commit -m "refactor: migrate shipped scenes to yaml assets"
```

## Task 9: Final verification sweep

**Files:**
- Modify: none
- Test: `tests/test_scene_definition.cpp`
- Test: `tests/test_yaml_scene_loader.cpp`
- Test: `tests/test_obj_mtl_importer.cpp`
- Test: `tests/test_scene_file_catalog.cpp`
- Test: `tests/test_scene_catalog.cpp`
- Test: `tests/test_shared_scene_builders.cpp`
- Test: `tests/test_offline_shared_scene_renderer.cpp`
- Test: `tests/test_cpu_scene_adapter.cpp`
- Test: `tests/test_realtime_scene_adapter.cpp`
- Test: `tests/test_realtime_scene_factory.cpp`
- Test: `tests/test_viewer_scene_reload.cpp`
- Test: `tests/test_viewer_scene_switch_controller.cpp`
- Test: `tests/test_viewer_quality_reference.cpp`
- Test: `render_realtime_viewer`

- [ ] **Step 1: Build the full affected target set**

Run:

```bash
cmake --build build --target \
  test_scene_definition \
  test_yaml_scene_loader \
  test_obj_mtl_importer \
  test_scene_file_catalog \
  test_scene_catalog \
  test_shared_scene_builders \
  test_offline_shared_scene_renderer \
  test_cpu_scene_adapter \
  test_realtime_scene_adapter \
  test_realtime_scene_factory \
  test_viewer_scene_reload \
  test_viewer_scene_switch_controller \
  test_viewer_quality_reference \
  render_realtime_viewer -j4
```

Expected: all targets build successfully.

- [ ] **Step 2: Run the verification suite**

Run:

```bash
ctest --test-dir build --output-on-failure -R \
  "test_scene_definition|test_yaml_scene_loader|test_obj_mtl_importer|test_scene_file_catalog|test_scene_catalog|test_shared_scene_builders|test_offline_shared_scene_renderer|test_cpu_scene_adapter|test_realtime_scene_adapter|test_realtime_scene_factory|test_viewer_scene_reload|test_viewer_scene_switch_controller|test_viewer_quality_reference"
```

Expected:

```text
100% tests passed
```

- [ ] **Step 3: Manual viewer smoke test**

Run: `./bin/render_realtime_viewer --scene final_room`

Expected:
- the scene combo opens wide enough to show all current scenes without mouse-wheel scrolling;
- editing `assets/scenes/final_room/scene.yaml` and pressing `Reload Current Scene` updates the current scene without recompiling;
- adding a new scene directory under `assets/scenes/` and pressing `Rescan Scene Directory` makes the new scene selectable;
- a malformed YAML edit shows an error message while the last good scene remains visible.

- [ ] **Step 4: Commit the verified end state**

```bash
git add .
git commit -m "feat: externalize scene descriptions with reloadable yaml catalog"
```

## Self-Review

- Spec coverage:
  - Editable scene files without recompilation: Tasks 3, 5, 7, 8.
  - CPU presets and realtime presets in the same source: Tasks 1, 3, 6, 8.
  - Manual reload and rescan plus automatic dirty reload support: Tasks 5 and 7.
  - Simple YAML format with optional includes: Tasks 3 and 4.
  - OBJ+MTL import: Tasks 2 and 4.
- Future renderer expansion space for lights/MIS: Task 2 extends `SceneIR`; Tasks 3 and 4 keep `SceneDefinition`/loader as the place where future `scene.lights` parsing can be added without changing adapters again.
  - OBJ+MTL reaching both CPU and realtime renderers: Tasks 2 and 4.
  - Larger scene switcher UI: Task 7.
- Placeholder scan:
  - No `TODO`, `TBD`, or “similar to task N” placeholders remain.
- Type consistency:
  - `SceneDefinition`, `SceneFileCatalog`, `load_scene_definition`, and `import_obj_mtl` use the same names across all tasks.
