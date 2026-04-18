#include "realtime/viewer/default_viewer_scene.h"

#include "realtime/frame_convention.h"

#include <Eigen/Core>

namespace rt::viewer {

SceneDescription make_final_room_scene() {
    SceneDescription scene;
    const int white = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.73, 0.73, 0.73}});
    const int green = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.30, 0.70, 0.35}});
    const int red = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.72, 0.25, 0.22}});
    const int blue = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.25, 0.35, 0.75}});
    const int light = scene.add_material(DiffuseLightMaterial {Eigen::Vector3d {12.0, 12.0, 12.0}});

    scene.add_quad(QuadPrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {-4.0, -1.0, -4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 8.0}),
        legacy_renderer_to_world(Eigen::Vector3d {8.0, 0.0, 0.0}), false});
    scene.add_quad(QuadPrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {-4.0, 3.5, -4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {8.0, 0.0, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 8.0}), false});
    scene.add_quad(QuadPrimitive {green, legacy_renderer_to_world(Eigen::Vector3d {-4.0, -1.0, -4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 4.5, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 8.0}), false});
    scene.add_quad(QuadPrimitive {red, legacy_renderer_to_world(Eigen::Vector3d {4.0, -1.0, -4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 8.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 4.5, 0.0}), false});
    scene.add_quad(QuadPrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {-4.0, -1.0, -4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {8.0, 0.0, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 4.5, 0.0}), false});
    scene.add_quad(QuadPrimitive {blue, legacy_renderer_to_world(Eigen::Vector3d {-4.0, -1.0, 4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 4.5, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {8.0, 0.0, 0.0}), false});
    scene.add_quad(QuadPrimitive {light, legacy_renderer_to_world(Eigen::Vector3d {-1.0, 3.15, -1.0}),
        legacy_renderer_to_world(Eigen::Vector3d {2.0, 0.0, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 2.0}), false});

    scene.add_quad(QuadPrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {-3.2, -0.25, -3.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 1.8}),
        legacy_renderer_to_world(Eigen::Vector3d {1.8, 0.0, 0.0}), false});
    scene.add_quad(QuadPrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {1.2, 0.15, 1.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 1.6}),
        legacy_renderer_to_world(Eigen::Vector3d {1.6, 0.0, 0.0}), false});
    scene.add_sphere(SpherePrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.25, -1.2}), 0.55, false});
    scene.add_sphere(SpherePrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {-1.6, 0.35, 1.7}), 0.55, false});
    scene.add_sphere(SpherePrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {-3.1, 1.0, 0.8}), 0.55, false});
    scene.add_sphere(SpherePrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {3.0, 1.35, -0.9}), 0.65, false});
    scene.add_sphere(SpherePrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {1.1, 1.1, -3.0}), 0.60, false});
    scene.add_sphere(SpherePrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {-0.8, 2.55, 2.2}), 0.45, false});
    scene.add_sphere(SpherePrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {0.9, 0.55, -0.1}), 0.45, false});
    scene.add_sphere(SpherePrimitive {white, legacy_renderer_to_world(Eigen::Vector3d {-0.35, 0.4, -1.15}), 0.35, false});
    return scene;
}

SceneDescription make_default_viewer_scene() {
    return make_final_room_scene();
}

RenderProfile default_viewer_profile() {
    return RenderProfile {
        .samples_per_pixel = 1,
        .max_bounces = 2,
        .enable_denoise = false,
        .rr_start_bounce = 2,
        .accumulation_reset_rotation_deg = 2.0,
        .accumulation_reset_translation = 0.05,
    };
}

}  // namespace rt::viewer
