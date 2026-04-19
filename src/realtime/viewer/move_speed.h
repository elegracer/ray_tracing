#pragma once

namespace rt::viewer {

double clamp_move_speed(double speed);
double apply_scroll_speed_step(double current_speed, double scroll_yoffset);

class MoveSpeedState {
public:
    explicit MoveSpeedState(double default_speed = 1.8);

    void reset(double default_speed);
    void apply_scroll(double scroll_yoffset);

    double current_speed() const;
    double default_speed() const;

private:
    double default_speed_ = 1.8;
    double current_speed_ = 1.8;
};

}  // namespace rt::viewer
