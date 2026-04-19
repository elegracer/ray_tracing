#include "realtime/viewer/move_speed.h"

#include <algorithm>
#include <cmath>

namespace rt::viewer {
namespace {

constexpr double kScrollStepMultiplier = 1.25;
constexpr double kMinMoveSpeed = 0.05;
constexpr double kMaxMoveSpeed = 5000.0;

}  // namespace

double clamp_move_speed(double speed) {
    return std::clamp(speed, kMinMoveSpeed, kMaxMoveSpeed);
}

double apply_scroll_speed_step(double current_speed, double scroll_yoffset) {
    if (scroll_yoffset == 0.0) {
        return clamp_move_speed(current_speed);
    }
    return clamp_move_speed(current_speed * std::pow(kScrollStepMultiplier, scroll_yoffset));
}

MoveSpeedState::MoveSpeedState(double default_speed) {
    reset(default_speed);
}

void MoveSpeedState::reset(double default_speed) {
    default_speed_ = clamp_move_speed(default_speed);
    current_speed_ = default_speed_;
}

void MoveSpeedState::apply_scroll(double scroll_yoffset) {
    current_speed_ = apply_scroll_speed_step(current_speed_, scroll_yoffset);
}

double MoveSpeedState::current_speed() const {
    return current_speed_;
}

double MoveSpeedState::default_speed() const {
    return default_speed_;
}

}  // namespace rt::viewer
