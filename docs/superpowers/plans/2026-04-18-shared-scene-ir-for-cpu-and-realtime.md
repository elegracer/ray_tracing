# Shared Scene IR For CPU And Realtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace all remaining `render_scene.cpp`-local scene implementations with a shared scene IR that both the CPU reference renderer and realtime renderer consume, so the same scene semantics work in `render_scene`, `render_realtime`, and `render_realtime_viewer`.

**Architecture:** Add a shared scene IR and scene-builder layer under `src/scene/`, then add two adapters: one that lowers IR into CPU `Texture` / `Material` / `Hittable` structures, and one that lowers IR into the realtime packed scene. Realtime support must be extended to understand texture-backed materials, box lowering, UV generation, and uniform media, because scene sharing is not real unless the realtime backend can render the migrated semantics.

**Tech Stack:** C++23, CMake, Eigen, OpenCV, custom CPU path tracer, OptiX/CUDA realtime pipeline, GLFW/OpenGL/ImGui viewer, existing CTest-style executable tests

---

### Task 1: Introduce The Shared Scene IR

**Files:**
- Create: `src/scene/shared_scene_ir.h`
- Create: `src/scene/shared_scene_ir.cpp`
- Create: `tests/test_shared_scene_ir.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
#include "scene/shared_scene_ir.h"
#include "test_support.h"

int main() {
    rt::scene::SceneIR scene;

    const int white = scene.add_texture(rt::scene::ConstantColorTextureDesc {
        .color = Eigen::Vector3d(0.73, 0.73, 0.73),
    });
    const int checker = scene.add_texture(rt::scene::CheckerTextureDesc {
        .scale = 0.32,
        .even_texture = white,
        .odd_texture = scene.add_texture(rt::scene::ConstantColorTextureDesc {
            .color = Eigen::Vector3d(0.2, 0.3, 0.1),
        }),
    });
    const int diffuse = scene.add_material(rt::scene::DiffuseMaterial {
        .albedo_texture = checker,
    });
    const int fog = scene.add_material(rt::scene::IsotropicVolumeMaterial {
        .albedo_texture = white,
    });
    const int sphere = scene.add_shape(rt::scene::SphereShape {
        .center = Eigen::Vector3d(0.0, 0.0, 0.0),
        .radius = 2.0,
    });

    scene.add_instance(rt::scene::SurfaceInstance {
        .shape_index = sphere,
        .material_index = diffuse,
        .transform = rt::scene::Transform::identity(),
    });
    scene.add_medium(rt::scene::MediumInstance {
        .shape_index = sphere,
        .material_index = fog,
        .density = 0.01,
        .transform = rt::scene::Transform::identity(),
    });

    expect_true(scene.textures().size() == 3, "texture count");
    expect_true(scene.materials().size() == 2, "material count");
    expect_true(scene.shapes().size() == 1, "shape count");
    expect_true(scene.surface_instances().size() == 1, "surface instance count");
    expect_true(scene.media().size() == 1, "medium count");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_shared_scene_ir`
Expected: FAIL because `scene/shared_scene_ir.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

`src/scene/shared_scene_ir.h`

```cpp
#pragma once

#include <Eigen/Core>

#include <string>
#include <variant>
#include <vector>

namespace rt::scene {

struct ConstantColorTextureDesc {
    Eigen::Vector3d color;
};

struct CheckerTextureDesc {
    double scale = 1.0;
    int even_texture = -1;
    int odd_texture = -1;
};

struct ImageTextureDesc {
    std::string path;
};

struct NoiseTextureDesc {
    double scale = 1.0;
};

using TextureDesc = std::variant<ConstantColorTextureDesc, CheckerTextureDesc, ImageTextureDesc, NoiseTextureDesc>;

struct DiffuseMaterial {
    int albedo_texture = -1;
};

struct MetalMaterial {
    int albedo_texture = -1;
    double fuzz = 0.0;
};

struct DielectricMaterial {
    double ior = 1.0;
};

struct EmissiveMaterial {
    int emission_texture = -1;
};

struct IsotropicVolumeMaterial {
    int albedo_texture = -1;
};

using MaterialDesc =
    std::variant<DiffuseMaterial, MetalMaterial, DielectricMaterial, EmissiveMaterial, IsotropicVolumeMaterial>;

struct SphereShape {
    Eigen::Vector3d center = Eigen::Vector3d::Zero();
    double radius = 1.0;
};

struct QuadShape {
    Eigen::Vector3d origin = Eigen::Vector3d::Zero();
    Eigen::Vector3d edge_u = Eigen::Vector3d::Zero();
    Eigen::Vector3d edge_v = Eigen::Vector3d::Zero();
};

struct BoxShape {
    Eigen::Vector3d min_corner = Eigen::Vector3d::Zero();
    Eigen::Vector3d max_corner = Eigen::Vector3d::Zero();
};

using ShapeDesc = std::variant<SphereShape, QuadShape, BoxShape>;

struct Transform {
    Eigen::Vector3d translation = Eigen::Vector3d::Zero();
    Eigen::Matrix3d rotation = Eigen::Matrix3d::Identity();

    static Transform identity();
};

struct SurfaceInstance {
    int shape_index = -1;
    int material_index = -1;
    Transform transform = Transform::identity();
};

struct MediumInstance {
    int shape_index = -1;
    int material_index = -1;
    double density = 0.0;
    Transform transform = Transform::identity();
};

class SceneIR {
public:
    int add_texture(const TextureDesc& texture);
    int add_material(const MaterialDesc& material);
    int add_shape(const ShapeDesc& shape);
    void add_instance(const SurfaceInstance& instance);
    void add_medium(const MediumInstance& medium);

    const std::vector<TextureDesc>& textures() const;
    const std::vector<MaterialDesc>& materials() const;
    const std::vector<ShapeDesc>& shapes() const;
    const std::vector<SurfaceInstance>& surface_instances() const;
    const std::vector<MediumInstance>& media() const;

private:
    std::vector<TextureDesc> textures_;
    std::vector<MaterialDesc> materials_;
    std::vector<ShapeDesc> shapes_;
    std::vector<SurfaceInstance> surface_instances_;
    std::vector<MediumInstance> media_;
};

}  // namespace rt::scene
```

`src/scene/shared_scene_ir.cpp`

```cpp
#include "scene/shared_scene_ir.h"

namespace rt::scene {

Transform Transform::identity() {
    return Transform {};
}

int SceneIR::add_texture(const TextureDesc& texture) {
    textures_.push_back(texture);
    return static_cast<int>(textures_.size()) - 1;
}

int SceneIR::add_material(const MaterialDesc& material) {
    materials_.push_back(material);
    return static_cast<int>(materials_.size()) - 1;
}

int SceneIR::add_shape(const ShapeDesc& shape) {
    shapes_.push_back(shape);
    return static_cast<int>(shapes_.size()) - 1;
}

void SceneIR::add_instance(const SurfaceInstance& instance) {
    surface_instances_.push_back(instance);
}

void SceneIR::add_medium(const MediumInstance& medium) {
    media_.push_back(medium);
}

const std::vector<TextureDesc>& SceneIR::textures() const { return textures_; }
const std::vector<MaterialDesc>& SceneIR::materials() const { return materials_; }
const std::vector<ShapeDesc>& SceneIR::shapes() const { return shapes_; }
const std::vector<SurfaceInstance>& SceneIR::surface_instances() const { return surface_instances_; }
const std::vector<MediumInstance>& SceneIR::media() const { return media_; }

}  // namespace rt::scene
```

`CMakeLists.txt`

```cmake
target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/shared_scene_ir.cpp
)

add_executable(test_shared_scene_ir)
target_sources(test_shared_scene_ir
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shared_scene_ir.cpp
)
target_link_libraries(test_shared_scene_ir PRIVATE core)
add_test(NAME test_shared_scene_ir COMMAND test_shared_scene_ir)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_shared_scene_ir && ctest --test-dir build -R test_shared_scene_ir --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/scene/shared_scene_ir.h src/scene/shared_scene_ir.cpp tests/test_shared_scene_ir.cpp
git commit -m "feat: add shared scene ir"
```

### Task 2: Add Shared Scene Builders And Migrate Catalog Ownership

**Files:**
- Create: `src/scene/shared_scene_builders.h`
- Create: `src/scene/shared_scene_builders.cpp`
- Create: `tests/test_shared_scene_builders.cpp`
- Modify: `src/realtime/scene_catalog.h`
- Modify: `src/realtime/scene_catalog.cpp`
- Modify: `src/realtime/realtime_scene_factory.h`
- Modify: `src/realtime/realtime_scene_factory.cpp`
- Modify: `utils/render_scene.cpp`
- Modify: `utils/render_realtime.cpp`
- Modify: `utils/render_realtime_viewer.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
#include "scene/shared_scene_builders.h"
#include "realtime/scene_catalog.h"
#include "test_support.h"

#include <array>
#include <string_view>

int main() {
    constexpr std::array<std::string_view, 16> expected_ids {
        "bouncing_spheres",
        "checkered_spheres",
        "earth_sphere",
        "perlin_spheres",
        "quads",
        "simple_light",
        "cornell_smoke",
        "cornell_smoke_extreme",
        "cornell_box",
        "cornell_box_extreme",
        "cornell_box_and_sphere",
        "cornell_box_and_sphere_extreme",
        "rttnw_final_scene",
        "rttnw_final_scene_extreme",
        "smoke",
        "final_room",
    };

    for (std::string_view id : expected_ids) {
        const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(id);
        expect_true(entry != nullptr, "catalog entry exists");

        const rt::scene::SceneIR scene = rt::scene::build_scene(id);
        expect_true(!scene.materials().empty(), "scene has materials");
        expect_true(!scene.shapes().empty(), "scene has shapes");
    }

    expect_true(rt::scene::scene_default_samples_per_pixel("cornell_box_extreme") == 10000, "extreme spp");
    expect_true(rt::scene::scene_default_samples_per_pixel("cornell_box") == 1000, "cornell default spp");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_shared_scene_builders`
Expected: FAIL because `scene/shared_scene_builders.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

`src/scene/shared_scene_builders.h`

```cpp
#pragma once

#include "scene/shared_scene_ir.h"

#include <string_view>

namespace rt::scene {

SceneIR build_scene(std::string_view scene_id);
int scene_default_samples_per_pixel(std::string_view scene_id);

}  // namespace rt::scene
```

`src/scene/shared_scene_builders.cpp`

```cpp
#include "scene/shared_scene_builders.h"

#include <stdexcept>

namespace rt::scene {
namespace {

SceneIR make_quads_scene() {
    SceneIR scene;
    const int red = scene.add_texture(ConstantColorTextureDesc {.color = Eigen::Vector3d(1.0, 0.2, 0.2)});
    const int green = scene.add_texture(ConstantColorTextureDesc {.color = Eigen::Vector3d(0.2, 1.0, 0.2)});
    const int blue = scene.add_texture(ConstantColorTextureDesc {.color = Eigen::Vector3d(0.2, 0.2, 1.0)});
    const int orange = scene.add_texture(ConstantColorTextureDesc {.color = Eigen::Vector3d(1.0, 0.5, 0.0)});
    const int teal = scene.add_texture(ConstantColorTextureDesc {.color = Eigen::Vector3d(0.2, 0.8, 0.8)});

    const int left = scene.add_material(DiffuseMaterial {.albedo_texture = red});
    const int back = scene.add_material(DiffuseMaterial {.albedo_texture = green});
    const int right = scene.add_material(DiffuseMaterial {.albedo_texture = blue});
    const int top = scene.add_material(DiffuseMaterial {.albedo_texture = orange});
    const int bottom = scene.add_material(DiffuseMaterial {.albedo_texture = teal});

    const int left_shape = scene.add_shape(QuadShape {
        .origin = Eigen::Vector3d(-3.0, -2.0, 5.0),
        .edge_u = Eigen::Vector3d(0.0, 0.0, -4.0),
        .edge_v = Eigen::Vector3d(0.0, 4.0, 0.0),
    });
    const int back_shape = scene.add_shape(QuadShape {
        .origin = Eigen::Vector3d(-2.0, -2.0, 0.0),
        .edge_u = Eigen::Vector3d(4.0, 0.0, 0.0),
        .edge_v = Eigen::Vector3d(0.0, 4.0, 0.0),
    });
    scene.add_instance(SurfaceInstance {.shape_index = left_shape, .material_index = left});
    scene.add_instance(SurfaceInstance {.shape_index = back_shape, .material_index = back});
    scene.add_instance(SurfaceInstance {
        .shape_index = scene.add_shape(QuadShape {
            .origin = Eigen::Vector3d(3.0, -2.0, 1.0),
            .edge_u = Eigen::Vector3d(0.0, 0.0, 4.0),
            .edge_v = Eigen::Vector3d(0.0, 4.0, 0.0),
        }),
        .material_index = right,
    });
    scene.add_instance(SurfaceInstance {
        .shape_index = scene.add_shape(QuadShape {
            .origin = Eigen::Vector3d(-2.0, 3.0, 1.0),
            .edge_u = Eigen::Vector3d(4.0, 0.0, 0.0),
            .edge_v = Eigen::Vector3d(0.0, 0.0, 4.0),
        }),
        .material_index = top,
    });
    scene.add_instance(SurfaceInstance {
        .shape_index = scene.add_shape(QuadShape {
            .origin = Eigen::Vector3d(-2.0, -3.0, 5.0),
            .edge_u = Eigen::Vector3d(4.0, 0.0, 0.0),
            .edge_v = Eigen::Vector3d(0.0, 0.0, -4.0),
        }),
        .material_index = bottom,
    });
    return scene;
}

SceneIR make_bouncing_spheres_scene();
SceneIR make_checkered_spheres_scene();
SceneIR make_earth_sphere_scene();
SceneIR make_perlin_spheres_scene();
SceneIR make_simple_light_scene();
SceneIR make_cornell_box_scene();
SceneIR make_cornell_box_and_sphere_scene();
SceneIR make_cornell_smoke_scene();
SceneIR make_rttnw_final_scene();
SceneIR make_realtime_smoke_scene();
SceneIR make_final_room_scene();

}  // namespace

SceneIR build_scene(std::string_view scene_id) {
    if (scene_id == "quads") {
        return make_quads_scene();
    }
    if (scene_id == "cornell_box" || scene_id == "cornell_box_extreme") {
        return make_cornell_box_scene();
    }
    if (scene_id == "cornell_box_and_sphere" || scene_id == "cornell_box_and_sphere_extreme") {
        return make_cornell_box_and_sphere_scene();
    }
    if (scene_id == "cornell_smoke" || scene_id == "cornell_smoke_extreme") {
        return make_cornell_smoke_scene();
    }
    if (scene_id == "bouncing_spheres") {
        return make_bouncing_spheres_scene();
    }
    if (scene_id == "checkered_spheres") {
        return make_checkered_spheres_scene();
    }
    if (scene_id == "earth_sphere") {
        return make_earth_sphere_scene();
    }
    if (scene_id == "perlin_spheres") {
        return make_perlin_spheres_scene();
    }
    if (scene_id == "simple_light") {
        return make_simple_light_scene();
    }
    if (scene_id == "rttnw_final_scene" || scene_id == "rttnw_final_scene_extreme") {
        return make_rttnw_final_scene();
    }
    if (scene_id == "smoke") {
        return make_realtime_smoke_scene();
    }
    if (scene_id == "final_room") {
        return make_final_room_scene();
    }
    throw std::invalid_argument("unknown shared scene id");
}

int scene_default_samples_per_pixel(std::string_view scene_id) {
    if (scene_id == "cornell_box_extreme" || scene_id == "cornell_box_and_sphere_extreme"
        || scene_id == "cornell_smoke_extreme" || scene_id == "rttnw_final_scene_extreme") {
        return 10000;
    }
    if (scene_id == "cornell_box" || scene_id == "cornell_box_and_sphere") {
        return 1000;
    }
    return 500;
}

}  // namespace rt::scene
```

Update `scene_catalog.cpp` so all scene ids are registered as both CPU and realtime capable after the adapters land:

```cpp
const std::vector<SceneCatalogEntry> kSceneCatalog {
    {"bouncing_spheres", "Bouncing Spheres", true, true},
    {"checkered_spheres", "Checkered Spheres", true, true},
    {"earth_sphere", "Earth Sphere", true, true},
    {"perlin_spheres", "Perlin Spheres", true, true},
    {"quads", "Quads", true, true},
    {"simple_light", "Simple Light", true, true},
    {"cornell_smoke", "Cornell Smoke", true, true},
    {"cornell_smoke_extreme", "Cornell Smoke Extreme", true, true},
    {"cornell_box", "Cornell Box", true, true},
    {"cornell_box_extreme", "Cornell Box Extreme", true, true},
    {"cornell_box_and_sphere", "Cornell Box And Sphere", true, true},
    {"cornell_box_and_sphere_extreme", "Cornell Box And Sphere Extreme", true, true},
    {"rttnw_final_scene", "RTTNW Final Scene", true, true},
    {"rttnw_final_scene_extreme", "RTTNW Final Scene Extreme", true, true},
    {"smoke", "Realtime Smoke", true, true},
    {"final_room", "Final Room", true, true},
};
```

Update `realtime_scene_factory.cpp` so it becomes a thin wrapper around the shared builders rather than the owner of scene definitions.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_shared_scene_builders && ctest --test-dir build -R "test_shared_scene_builders|test_scene_catalog" --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/scene/shared_scene_builders.h src/scene/shared_scene_builders.cpp src/realtime/scene_catalog.h src/realtime/scene_catalog.cpp src/realtime/realtime_scene_factory.h src/realtime/realtime_scene_factory.cpp tests/test_shared_scene_builders.cpp utils/render_scene.cpp utils/render_realtime.cpp utils/render_realtime_viewer.cpp
git commit -m "feat: add shared scene builders"
```

### Task 3: Add The CPU Scene Adapter And Migrate `render_scene`

**Files:**
- Create: `src/scene/cpu_scene_adapter.h`
- Create: `src/scene/cpu_scene_adapter.cpp`
- Create: `src/core/offline_shared_scene_renderer.h`
- Create: `src/core/offline_shared_scene_renderer.cpp`
- Create: `tests/test_cpu_scene_adapter.cpp`
- Modify: `utils/render_scene.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
#include "scene/cpu_scene_adapter.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

int main() {
    {
        const rt::scene::SceneIR scene = rt::scene::build_scene("checkered_spheres");
        const rt::scene::CpuSceneAdapterResult adapted = rt::scene::adapt_to_cpu(scene);
        expect_true(adapted.world != nullptr, "checker world");
    }
    {
        const rt::scene::SceneIR scene = rt::scene::build_scene("earth_sphere");
        const rt::scene::CpuSceneAdapterResult adapted = rt::scene::adapt_to_cpu(scene);
        expect_true(adapted.world != nullptr, "earth world");
    }
    {
        const rt::scene::SceneIR scene = rt::scene::build_scene("cornell_smoke");
        const rt::scene::CpuSceneAdapterResult adapted = rt::scene::adapt_to_cpu(scene);
        expect_true(adapted.world != nullptr, "smoke world");
    }
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_cpu_scene_adapter`
Expected: FAIL because the CPU adapter types do not exist yet.

- [ ] **Step 3: Write minimal implementation**

`src/scene/cpu_scene_adapter.h`

```cpp
#pragma once

#include "scene/shared_scene_ir.h"

#include "common/hittable.h"

namespace rt::scene {

struct CpuSceneAdapterResult {
    pro::proxy<Hittable> world;
};

CpuSceneAdapterResult adapt_to_cpu(const SceneIR& scene);

}  // namespace rt::scene
```

`src/scene/cpu_scene_adapter.cpp`

```cpp
#include "scene/cpu_scene_adapter.h"

#include "common/constant_medium.h"
#include "common/hittable_list.h"
#include "common/hittable.h"
#include "common/material.h"
#include "common/quad.h"
#include "common/sphere.h"

#include <numbers>

namespace rt::scene {
namespace {

pro::proxy<Texture> build_texture(const SceneIR& scene, int index) {
    return std::visit(
        [&](const auto& texture) -> pro::proxy<Texture> {
            using T = std::decay_t<decltype(texture)>;
            if constexpr (std::is_same_v<T, ConstantColorTextureDesc>) {
                return pro::make_proxy_shared<Texture, SolidColor>(texture.color);
            } else if constexpr (std::is_same_v<T, CheckerTextureDesc>) {
                return pro::make_proxy_shared<Texture, ::CheckerTexture>(
                    texture.scale, build_texture(scene, texture.even_texture), build_texture(scene, texture.odd_texture));
            } else if constexpr (std::is_same_v<T, ImageTextureDesc>) {
                return pro::make_proxy_shared<Texture, ::ImageTexture>(texture.path);
            } else {
                return pro::make_proxy_shared<Texture, ::NoiseTexture>(texture.scale);
            }
        },
        scene.textures().at(static_cast<std::size_t>(index)));
}

pro::proxy<Material> build_material(const SceneIR& scene, int index) {
    return std::visit(
        [&](const auto& material) -> pro::proxy<Material> {
            using T = std::decay_t<decltype(material)>;
            if constexpr (std::is_same_v<T, DiffuseMaterial>) {
                return pro::make_proxy_shared<Material, Lambertion>(build_texture(scene, material.albedo_texture));
            } else if constexpr (std::is_same_v<T, MetalMaterial>) {
                const auto tex = std::get<ConstantColorTextureDesc>(
                    scene.textures().at(static_cast<std::size_t>(material.albedo_texture)));
                return pro::make_proxy_shared<Material, Metal>(tex.color, material.fuzz);
            } else if constexpr (std::is_same_v<T, DielectricMaterial>) {
                return pro::make_proxy_shared<Material, Dielectric>(material.ior);
            } else if constexpr (std::is_same_v<T, EmissiveMaterial>) {
                return pro::make_proxy_shared<Material, DiffuseLight>(build_texture(scene, material.emission_texture));
            } else {
                return pro::make_proxy_shared<Material, Isotropic>(build_texture(scene, material.albedo_texture));
            }
        },
        scene.materials().at(static_cast<std::size_t>(index)));
}

pro::proxy<Hittable> apply_transform(pro::proxy<Hittable> object, const Transform& transform) {
    if (!transform.rotation.isApprox(Eigen::Matrix3d::Identity())) {
        const double yaw_deg =
            std::atan2(transform.rotation(0, 2), transform.rotation(0, 0)) * 180.0 / std::numbers::pi;
        object = pro::make_proxy_shared<Hittable, RotateY>(object, yaw_deg);
    }
    if (!transform.translation.isZero(0.0)) {
        object = pro::make_proxy_shared<Hittable, Translate>(object, transform.translation);
    }
    return object;
}

}  // namespace

CpuSceneAdapterResult adapt_to_cpu(const SceneIR& scene) {
    auto world = std::make_shared<HittableList>();
    for (const SurfaceInstance& instance : scene.surface_instances()) {
        const pro::proxy<Material> material = build_material(scene, instance.material_index);
        std::visit(
            [&](const auto& shape) {
                using T = std::decay_t<decltype(shape)>;
                pro::proxy<Hittable> object;
                if constexpr (std::is_same_v<T, SphereShape>) {
                    object = pro::make_proxy_shared<Hittable, Sphere>(shape.center, shape.radius, material);
                } else if constexpr (std::is_same_v<T, QuadShape>) {
                    object = pro::make_proxy_shared<Hittable, Quad>(shape.origin, shape.edge_u, shape.edge_v, material);
                } else {
                    object = box(shape.min_corner, shape.max_corner, material);
                }
                world->add(apply_transform(object, instance.transform));
            },
            scene.shapes().at(static_cast<std::size_t>(instance.shape_index)));
    }
    for (const MediumInstance& medium : scene.media()) {
        const auto& volume_material = std::get<IsotropicVolumeMaterial>(
            scene.materials().at(static_cast<std::size_t>(medium.material_index)));
        const pro::proxy<Material> empty = pro::make_proxy_shared<Material, EmptyMaterial>();
        std::visit(
            [&](const auto& shape) {
                using T = std::decay_t<decltype(shape)>;
                pro::proxy<Hittable> boundary;
                if constexpr (std::is_same_v<T, SphereShape>) {
                    boundary = pro::make_proxy_shared<Hittable, Sphere>(shape.center, shape.radius, empty);
                } else if constexpr (std::is_same_v<T, QuadShape>) {
                    boundary = pro::make_proxy_shared<Hittable, Quad>(shape.origin, shape.edge_u, shape.edge_v, empty);
                } else {
                    boundary = box(shape.min_corner, shape.max_corner, empty);
                }
                boundary = apply_transform(boundary, medium.transform);
                world->add(pro::make_proxy_shared<Hittable, ConstantMedium>(
                    boundary, medium.density, build_texture(scene, volume_material.albedo_texture)));
            },
            scene.shapes().at(static_cast<std::size_t>(medium.shape_index)));
    }
    return CpuSceneAdapterResult {.world = world};
}

}  // namespace rt::scene
```

`src/core/offline_shared_scene_renderer.cpp`

```cpp
#include "core/offline_shared_scene_renderer.h"

#include "common/camera.h"
#include "scene/cpu_scene_adapter.h"
#include "scene/shared_scene_builders.h"

namespace rt {

cv::Mat render_shared_scene(std::string_view scene_id, int samples_per_pixel) {
    const scene::SceneIR scene_ir = scene::build_scene(scene_id);
    const scene::CpuSceneAdapterResult adapted = scene::adapt_to_cpu(scene_ir);

    Camera cam;
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = 50;
    cam.render(adapted.world);
    return cam.img.clone();
}

}  // namespace rt
```

Update `utils/render_scene.cpp` to become only CLI parsing plus a call into `render_shared_scene(scene_id, scene_default_samples_per_pixel(scene_id))`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_cpu_scene_adapter render_scene && ctest --test-dir build -R "test_cpu_scene_adapter|test_cpu_render_smoke" --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/scene/cpu_scene_adapter.h src/scene/cpu_scene_adapter.cpp src/core/offline_shared_scene_renderer.h src/core/offline_shared_scene_renderer.cpp tests/test_cpu_scene_adapter.cpp utils/render_scene.cpp
git commit -m "feat: add cpu adapter for shared scenes"
```

### Task 4: Extend Realtime Scene Data For Textures, UVs, Boxes, And Media

**Files:**
- Modify: `src/realtime/scene_description.h`
- Modify: `src/realtime/scene_description.cpp`
- Modify: `src/realtime/gpu/launch_params.h`
- Modify: `src/realtime/gpu/programs.cu`
- Modify: `src/realtime/gpu/optix_renderer.h`
- Modify: `src/realtime/gpu/optix_renderer.cpp`
- Create: `tests/test_realtime_texture_materials.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
#include "realtime/scene_description.h"
#include "realtime/gpu/optix_renderer.h"
#include "test_support.h"

int main() {
    rt::SceneDescription scene;
    const int white = scene.add_texture(rt::ConstantColorTextureDesc {
        .color = Eigen::Vector3d(1.0, 1.0, 1.0),
    });
    const int checker = scene.add_texture(rt::CheckerTextureDesc {
        .scale = 0.32,
        .even_texture = white,
        .odd_texture = scene.add_texture(rt::ConstantColorTextureDesc {
            .color = Eigen::Vector3d(0.0, 0.0, 0.0),
        }),
    });
    const int diffuse = scene.add_material(rt::LambertianMaterial {
        .albedo_texture = checker,
    });

    scene.add_sphere(rt::SpherePrimitive {
        .material_index = diffuse,
        .center = rt::legacy_renderer_to_world(Eigen::Vector3d(0.0, 0.0, -3.0)),
        .radius = 1.0,
        .dynamic = false,
    });

    const rt::PackedScene packed = scene.pack();
    expect_true(packed.texture_count == 3, "texture count");
    expect_true(packed.material_count == 1, "material count");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_realtime_texture_materials`
Expected: FAIL because realtime scene textures do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Update `src/realtime/scene_description.h` to add texture descriptors and texture-backed material fields:

```cpp
struct ConstantColorTextureDesc {
    Eigen::Vector3d color;
};

struct CheckerTextureDesc {
    double scale;
    int even_texture;
    int odd_texture;
};

struct ImageTextureDesc {
    std::string path;
};

struct NoiseTextureDesc {
    double scale;
};

using TextureDesc = std::variant<ConstantColorTextureDesc, CheckerTextureDesc, ImageTextureDesc, NoiseTextureDesc>;

struct LambertianMaterial {
    int albedo_texture = -1;
};

struct MetalMaterial {
    int albedo_texture = -1;
    double fuzz;
};

struct DiffuseLightMaterial {
    int emission_texture = -1;
};
```

Add texture storage and `texture_count` to `PackedScene`, plus an `add_texture` entrypoint to `SceneDescription`.

Update `launch_params.h` and `optix_renderer.cpp` to upload packed textures and make them visible to the kernel.

In `programs.cu`, add:

- procedural texture evaluation helpers for constant / checker / noise
- image texture sampling helper
- UV generation for spheres and quads
- material evaluation through texture indices instead of only flat colors

Boxes should still lower to quads during adapter translation, not become a new GPU primitive.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_realtime_texture_materials && ctest --test-dir build -R "test_realtime_texture_materials|test_optix_path_trace|test_optix_profiled_render" --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/scene_description.h src/realtime/scene_description.cpp src/realtime/gpu/launch_params.h src/realtime/gpu/programs.cu src/realtime/gpu/optix_renderer.h src/realtime/gpu/optix_renderer.cpp tests/test_realtime_texture_materials.cpp
git commit -m "feat: add realtime texture-backed materials"
```

### Task 5: Add Realtime Uniform Media And The Shared Realtime Adapter

**Files:**
- Create: `src/scene/realtime_scene_adapter.h`
- Create: `src/scene/realtime_scene_adapter.cpp`
- Create: `tests/test_realtime_scene_adapter.cpp`
- Modify: `src/realtime/scene_description.h`
- Modify: `src/realtime/scene_description.cpp`
- Modify: `src/realtime/gpu/launch_params.h`
- Modify: `src/realtime/gpu/programs.cu`
- Modify: `src/realtime/gpu/optix_renderer.cpp`
- Modify: `src/realtime/realtime_scene_factory.h`
- Modify: `src/realtime/realtime_scene_factory.cpp`
- Modify: `utils/render_realtime.cpp`
- Modify: `utils/render_realtime_viewer.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
#include "scene/realtime_scene_adapter.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

int main() {
    {
        const rt::scene::SceneIR scene = rt::scene::build_scene("earth_sphere");
        const rt::SceneDescription realtime = rt::scene::adapt_to_realtime(scene);
        const rt::PackedScene packed = realtime.pack();
        expect_true(packed.texture_count >= 1, "earth texture exported");
        expect_true(packed.sphere_count >= 1, "earth sphere exported");
    }
    {
        const rt::scene::SceneIR scene = rt::scene::build_scene("cornell_smoke");
        const rt::SceneDescription realtime = rt::scene::adapt_to_realtime(scene);
        const rt::PackedScene packed = realtime.pack();
        expect_true(packed.medium_count >= 1, "smoke medium exported");
    }
    {
        const rt::scene::SceneIR scene = rt::scene::build_scene("rttnw_final_scene");
        const rt::SceneDescription realtime = rt::scene::adapt_to_realtime(scene);
        const rt::PackedScene packed = realtime.pack();
        expect_true(packed.quad_count >= 6, "box ground lowered to quads");
    }
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_realtime_scene_adapter`
Expected: FAIL because there is no shared realtime adapter or realtime medium support yet.

- [ ] **Step 3: Write minimal implementation**

`src/scene/realtime_scene_adapter.h`

```cpp
#pragma once

#include "realtime/scene_description.h"
#include "scene/shared_scene_ir.h"

namespace rt::scene {

SceneDescription adapt_to_realtime(const SceneIR& scene);

}  // namespace rt::scene
```

`src/scene/realtime_scene_adapter.cpp`

```cpp
#include "scene/realtime_scene_adapter.h"

#include "realtime/frame_convention.h"

namespace rt::scene {
namespace {

TextureDesc convert_texture(const ConstantColorTextureDesc& texture);
TextureDesc convert_texture(const CheckerTextureDesc& texture);
TextureDesc convert_texture(const ImageTextureDesc& texture);
TextureDesc convert_texture(const NoiseTextureDesc& texture);
MaterialDesc convert_material(const DiffuseMaterial& material);
MaterialDesc convert_material(const MetalMaterial& material);
MaterialDesc convert_material(const DielectricMaterial& material);
MaterialDesc convert_material(const EmissiveMaterial& material);
MaterialDesc convert_material(const IsotropicVolumeMaterial& material);
void emit_realtime_surface(const SceneIR& scene, const SurfaceInstance& instance, SceneDescription& out);
void emit_realtime_medium(const SceneIR& scene, const MediumInstance& medium, SceneDescription& out);

}  // namespace

SceneDescription adapt_to_realtime(const SceneIR& scene) {
    SceneDescription out;
    for (const TextureDesc& texture : scene.textures()) {
        std::visit([&](const auto& t) { out.add_texture(convert_texture(t)); }, texture);
    }
    for (const MaterialDesc& material : scene.materials()) {
        std::visit([&](const auto& m) { out.add_material(convert_material(m)); }, material);
    }
    for (const SurfaceInstance& instance : scene.surface_instances()) {
        emit_realtime_surface(scene, instance, out);
    }
    for (const MediumInstance& medium : scene.media()) {
        emit_realtime_medium(scene, medium, out);
    }
    return out;
}

}  // namespace rt::scene
```

Update realtime scene types to add `MediumPrimitive`:

```cpp
struct MediumPrimitive {
    int material_index;
    int boundary_shape_type;
    Eigen::Vector3d center;
    Eigen::Vector3d half_extents;
    double radius;
    double density;
};
```

Update `programs.cu` so volume rendering handles homogeneous media inside sphere and box boundaries using isotropic scattering, matching the shared IR semantics.

Update `realtime_scene_factory.cpp` to become a thin wrapper:

```cpp
SceneDescription make_realtime_scene(std::string_view scene_id) {
    return scene::adapt_to_realtime(scene::build_scene(scene_id));
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_realtime_scene_adapter render_realtime render_realtime_viewer && ctest --test-dir build -R "test_realtime_scene_adapter|test_render_realtime_cli|test_render_realtime_final_room_quality_cli" --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/scene/realtime_scene_adapter.h src/scene/realtime_scene_adapter.cpp src/realtime/scene_description.h src/realtime/scene_description.cpp src/realtime/gpu/launch_params.h src/realtime/gpu/programs.cu src/realtime/gpu/optix_renderer.cpp src/realtime/realtime_scene_factory.h src/realtime/realtime_scene_factory.cpp tests/test_realtime_scene_adapter.cpp utils/render_realtime.cpp utils/render_realtime_viewer.cpp
git commit -m "feat: add realtime adapter for shared scenes"
```

### Task 6: Migrate The Remaining Scene Builders And Add Cross-Entry Regression Tests

**Files:**
- Modify: `src/scene/shared_scene_builders.cpp`
- Create: `tests/test_shared_scene_regression.cpp`
- Modify: `tests/test_reference_vs_realtime.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
#include "scene/shared_scene_builders.h"
#include "scene/realtime_scene_adapter.h"
#include "test_support.h"

#include <array>
#include <string_view>

int main() {
    constexpr std::array<std::string_view, 5> representative_scenes {
        "quads",
        "earth_sphere",
        "cornell_smoke",
        "bouncing_spheres",
        "rttnw_final_scene",
    };

    for (std::string_view id : representative_scenes) {
        const rt::scene::SceneIR scene = rt::scene::build_scene(id);
        expect_true(!scene.materials().empty(), "shared builder materials");
        expect_true(!scene.shapes().empty(), "shared builder shapes");

        const rt::SceneDescription realtime = rt::scene::adapt_to_realtime(scene);
        const rt::PackedScene packed = realtime.pack();
        expect_true(packed.material_count > 0, "packed materials");
        expect_true(packed.sphere_count + packed.quad_count > 0, "packed geometry");
    }
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_shared_scene_regression`
Expected: FAIL until all builders are completed and wired.

- [ ] **Step 3: Write minimal implementation**

Complete `shared_scene_builders.cpp` so every current `render_scene.cpp` scene is produced from shared IR:

- `bouncing_spheres`
- `checkered_spheres`
- `earth_sphere`
- `perlin_spheres`
- `quads`
- `simple_light`
- `cornell_smoke`
- `cornell_smoke_extreme`
- `cornell_box`
- `cornell_box_extreme`
- `cornell_box_and_sphere`
- `cornell_box_and_sphere_extreme`
- `rttnw_final_scene`
- `rttnw_final_scene_extreme`
- `smoke`
- `final_room`

For the motion-bearing scenes, explicitly remove motion and keep static approximations in the builder code.

Update `tests/test_reference_vs_realtime.cpp` to compare at least one texture scene and one medium scene after the shared IR migration:

```cpp
const rt::scene::SceneIR earth_scene = rt::scene::build_scene("earth_sphere");
const rt::SceneDescription realtime_earth = rt::scene::adapt_to_realtime(earth_scene);
expect_true(realtime_earth.pack().texture_count >= 1, "earth texture survives realtime path");

const rt::scene::SceneIR smoke_scene = rt::scene::build_scene("cornell_smoke");
const rt::SceneDescription realtime_smoke = rt::scene::adapt_to_realtime(smoke_scene);
expect_true(realtime_smoke.pack().medium_count >= 1, "smoke medium survives realtime path");
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_shared_scene_regression test_reference_vs_realtime`
Expected: PASS

Run: `ctest --test-dir build -R "test_shared_scene_regression|test_reference_vs_realtime|test_render_realtime_cli|test_render_realtime_final_room_quality_cli" --output-on-failure`
Expected: PASS

Run manually: `./bin/render_realtime_viewer --scene earth_sphere`
Expected:
- viewer starts
- earth scene loads
- scene switcher lists all migrated scenes

Run manually: `./bin/render_realtime_viewer --scene cornell_smoke`
Expected:
- viewer starts
- smoke scene loads
- no runtime scene-adapter error appears

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/scene/shared_scene_builders.cpp tests/test_shared_scene_regression.cpp tests/test_reference_vs_realtime.cpp
git commit -m "feat: migrate remaining scenes to shared ir"
```

## Self-Review Checklist

- [ ] Confirm every scene id previously owned by `utils/render_scene.cpp` is now built through `scene::build_scene`
- [ ] Confirm moving-sphere behavior was removed explicitly rather than silently ignored
- [ ] Confirm the CPU path renders from shared IR rather than duplicating scene construction
- [ ] Confirm the realtime path renders from shared IR rather than duplicating scene construction
- [ ] Confirm textures, media, boxes, and transforms are represented in the shared scene layer
- [ ] Confirm boxes are lowered in the adapter layers instead of becoming backend-specific hacks in the builders
- [ ] Confirm the realtime backend was extended enough that texture and medium scenes are actually renderable, not merely registered
- [ ] Confirm the catalog now truthfully marks the migrated scenes as usable in all three entrypoints
