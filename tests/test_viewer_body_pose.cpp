#include "realtime/viewer/body_pose.h"
#include "realtime/frame_convention.h"
#include "test_support.h"

#include <cmath>

int main() {
    {
        const rt::viewer::BodyPose spawn = rt::viewer::default_spawn_pose();
        expect_vec3_near(spawn.position, Eigen::Vector3d(0.0, -0.8, 0.35), 1e-12, "spawn position");
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
        expect_vec3_near(rt::viewer::forward_direction(neutral), Eigen::Vector3d(0.0, 1.0, 0.0), 1e-12,
            "neutral forward");
        expect_vec3_near(rt::viewer::right_direction(neutral), Eigen::Vector3d(1.0, 0.0, 0.0), 1e-12,
            "neutral right");
    }

    {
        const rt::viewer::BodyPose yawed {.position = Eigen::Vector3d::Zero(), .yaw_deg = 90.0, .pitch_deg = 0.0};
        expect_vec3_near(rt::viewer::forward_direction(yawed), Eigen::Vector3d(-1.0, 0.0, 0.0), 1e-12,
            "yaw 90 forward");
        expect_vec3_near(rt::viewer::right_direction(yawed), Eigen::Vector3d(0.0, 1.0, 0.0), 1e-12,
            "yaw 90 right");
    }

    {
        const rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 90.0, .pitch_deg = 45.0};
        const Eigen::Vector3d forward = rt::viewer::forward_direction(pose);
        expect_true(forward.x() < -0.7, "yaw+pitch forward x");
        expect_true(std::abs(forward.y()) < 1e-12, "yaw+pitch forward y near zero");
        expect_true(forward.z() > 0.7, "yaw+pitch forward z");
    }

    {
        rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 0.0, .pitch_deg = 45.0};
        rt::viewer::integrate_wasd(pose, true, false, false, false, false, false, 2.0);
        const Eigen::Vector3d expected = Eigen::Vector3d(0.0, std::sqrt(2.0), std::sqrt(2.0));
        expect_vec3_near(pose.position, expected, 1e-12, "pitched forward movement");
    }

    {
        rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 0.0, .pitch_deg = 10.0};
        rt::viewer::integrate_mouse_look(pose, 2.0, 3.0, 4.0);
        expect_near(pose.yaw_deg, 8.0, 1e-12, "mouse look yaw sign");
        expect_near(pose.pitch_deg, -2.0, 1e-12, "mouse look pitch sign");
    }

    {
        rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 0.0, .pitch_deg = 79.0};
        rt::viewer::integrate_mouse_look(pose, 0.0, -5.0, 1.0);
        expect_near(pose.pitch_deg, 80.0, 1e-12, "mouse look upper clamp");

        rt::viewer::integrate_mouse_look(pose, 0.0, 500.0, 1.0);
        expect_near(pose.pitch_deg, -80.0, 1e-12, "mouse look lower clamp");
    }

    {
        rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 0.0, .pitch_deg = 0.0};
        rt::viewer::integrate_wasd(pose, true, false, false, true, false, false, 2.0);
        const Eigen::Vector3d expected = Eigen::Vector3d(std::sqrt(2.0), std::sqrt(2.0), 0.0);
        expect_vec3_near(pose.position, expected, 1e-12, "wasd normalized diagonal");

        rt::viewer::integrate_wasd(pose, false, true, true, false, false, false, 2.0);
        expect_vec3_near(pose.position, Eigen::Vector3d::Zero(), 1e-12, "wasd opposite diagonal returns");
    }

    {
        const rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 15.0, .pitch_deg = 20.0};
        expect_true(rt::viewer::forward_direction(pose).z() > 0.3, "world forward keeps pitch");
        expect_true(rt::viewer::right_direction(pose).x() > 0.9, "world right matches visible right");
    }

    {
        rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 0.0, .pitch_deg = 0.0};
        rt::viewer::integrate_wasd(pose, false, false, false, false, true, false, 2.0);
        expect_vec3_near(pose.position, Eigen::Vector3d(0.0, 0.0, -2.0), 1e-12, "Q moves down");

        rt::viewer::integrate_wasd(pose, false, false, false, false, false, true, 2.0);
        expect_vec3_near(pose.position, Eigen::Vector3d::Zero(), 1e-12, "E moves up");
    }

    return 0;
}
