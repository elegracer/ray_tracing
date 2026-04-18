#include "realtime/viewer/body_pose.h"
#include "test_support.h"

#include <cmath>

int main() {
    {
        const rt::viewer::BodyPose spawn = rt::viewer::default_spawn_pose();
        expect_vec3_near(spawn.position, Eigen::Vector3d(0.0, 0.35, 0.8), 1e-12, "spawn position");
        expect_near(spawn.yaw_deg, 0.0, 1e-12, "spawn yaw");
        expect_near(spawn.pitch_deg, 0.0, 1e-12, "spawn pitch");
    }

    {
        expect_near(rt::viewer::clamp_pitch_deg(-200.0), -80.0, 1e-12, "pitch clamp low");
        expect_near(rt::viewer::clamp_pitch_deg(30.0), 30.0, 1e-12, "pitch clamp passthrough");
        expect_near(rt::viewer::clamp_pitch_deg(200.0), 80.0, 1e-12, "pitch clamp high");
    }

    {
        const rt::viewer::BodyPose neutral {.position = Eigen::Vector3d::Zero(), .yaw_deg = 0.0, .pitch_deg = 0.0};
        expect_vec3_near(rt::viewer::forward_direction(neutral), Eigen::Vector3d(0.0, 0.0, -1.0), 1e-12,
            "neutral forward");
        expect_vec3_near(rt::viewer::right_direction(neutral), Eigen::Vector3d(1.0, 0.0, 0.0), 1e-12,
            "neutral right");
    }

    {
        rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 0.0, .pitch_deg = 45.0};
        rt::viewer::integrate_wasd(pose, true, false, false, false, 2.0);
        const Eigen::Vector3d expected = Eigen::Vector3d(0.0, std::sqrt(2.0), -std::sqrt(2.0));
        expect_vec3_near(pose.position, expected, 1e-12, "pitched forward movement");
    }

    {
        rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 0.0, .pitch_deg = 0.0};
        rt::viewer::integrate_wasd(pose, true, false, false, true, 2.0);
        const Eigen::Vector3d expected = Eigen::Vector3d(std::sqrt(2.0), 0.0, -std::sqrt(2.0));
        expect_vec3_near(pose.position, expected, 1e-12, "wasd normalized diagonal");

        rt::viewer::integrate_wasd(pose, false, true, true, false, 2.0);
        expect_vec3_near(pose.position, Eigen::Vector3d::Zero(), 1e-12, "wasd opposite diagonal returns");
    }

    return 0;
}
