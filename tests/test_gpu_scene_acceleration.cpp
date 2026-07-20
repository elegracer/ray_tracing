#include "realtime/gpu/gpu_scene_acceleration.h"
#include "realtime/gpu/packed_scene_preparation.h"
#include "realtime/scene_description.h"
#include "test_support.h"

namespace {

rt::PackedScene make_scene(float second_center_x, bool add_third, float albedo) {
    rt::SceneDescription scene;
    const int material =
        scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {albedo, 0.2, 0.1}});
    scene.add_sphere(rt::SpherePrimitive {
        .material_index = material,
        .center = Eigen::Vector3d {-1.0, 0.0, -2.0},
        .radius = 0.5,
        .dynamic = false,
        .acceleration_prototype_id = 7,
        .acceleration_instance_id = 10,
    });
    scene.add_sphere(rt::SpherePrimitive {
        .material_index = material,
        .center = Eigen::Vector3d {second_center_x, 0.0, -2.0},
        .radius = 0.5,
        .dynamic = true,
        .acceleration_prototype_id = 7,
        .acceleration_instance_id = 11,
    });
    if (add_third) {
        scene.add_sphere(rt::SpherePrimitive {
            .material_index = material,
            .center = Eigen::Vector3d {2.0, 0.0, -2.0},
            .radius = 0.5,
            .dynamic = false,
            .acceleration_prototype_id = 8,
            .acceleration_instance_id = 12,
        });
    }
    return scene.pack();
}

} // namespace

int main() {
    rt::GpuSceneAcceleration acceleration;

    const rt::GpuPreparedScene initial = rt::prepare_gpu_scene(make_scene(1.0f, false, 0.7f));
    const rt::AccelerationUpdateStats rebuild = acceleration.update(initial);
    expect_true(rebuild.kind == rt::AccelerationUpdateKind::rebuild, "initial AS rebuild");
    expect_true(rebuild.node_count == 1, "two primitives share one BVH leaf");
    expect_true(rebuild.primitive_reference_count == 2, "two BVH primitive references");
    expect_true(rebuild.prototype_count == 1, "one shared prototype");
    expect_true(rebuild.instance_count == 2, "two top-level instances");
    expect_true(rebuild.instanced_primitive_count == 2, "instanced primitives observed");
    expect_true(rebuild.generation == 1, "rebuild advances generation");

    const rt::AccelerationUpdateStats reuse = acceleration.update(initial);
    expect_true(reuse.kind == rt::AccelerationUpdateKind::reuse, "unchanged AS reuse");
    expect_true(reuse.generation == rebuild.generation, "reuse preserves generation");

    const rt::GpuPreparedScene material_update =
        rt::prepare_gpu_scene(make_scene(1.0f, false, 0.9f));
    const rt::AccelerationUpdateStats update = acceleration.update(material_update);
    expect_true(update.kind == rt::AccelerationUpdateKind::update, "material-only AS update");
    expect_true(update.generation == 2, "update advances generation");

    const rt::GpuPreparedScene moved = rt::prepare_gpu_scene(make_scene(1.5f, false, 0.9f));
    const rt::AccelerationUpdateStats refit = acceleration.update(moved);
    expect_true(refit.kind == rt::AccelerationUpdateKind::refit, "same-topology AS refit");
    expect_true(refit.node_count == rebuild.node_count, "refit preserves BVH topology");
    expect_true(refit.generation == 3, "refit advances generation");
    expect_true(acceleration.nodes().front().bounds_max.x() >= 2.0f,
        "refit expands root bounds around moved sphere");

    const rt::GpuPreparedScene topology_change =
        rt::prepare_gpu_scene(make_scene(1.5f, true, 0.9f));
    const rt::AccelerationUpdateStats second_rebuild = acceleration.update(topology_change);
    expect_true(second_rebuild.kind == rt::AccelerationUpdateKind::rebuild,
        "topology change rebuilds AS");
    expect_true(second_rebuild.primitive_reference_count == 3, "rebuild includes added primitive");
    expect_true(second_rebuild.prototype_count == 2, "rebuild preserves prototype identities");
    expect_true(second_rebuild.instance_count == 3, "rebuild preserves instance identities");
    expect_true(second_rebuild.generation == 4, "second rebuild advances generation");

    return 0;
}
