#include "core/movement.h"

#include <algorithm>
#include <cmath>

namespace mk {

static const double kVecX[4] = {0, 0, -1, 1};
static const double kVecY[4] = {-1, 1, 0, 0};

void Movement::configure(double initial_speed, double acceleration, double max_speed) {
  initial_ = initial_speed;
  accel_ = acceleration;
  max_ = max_speed;
}

void Movement::set_scale(double sx, double sy) {
  sx_ = sx;
  sy_ = sy;
}

void Movement::press(Dir d, double now) {
  int i = static_cast<int>(d);
  if (press_time_[i] < 0) press_time_[i] = now;
}

void Movement::release(Dir d) { press_time_[static_cast<int>(d)] = -1; }

void Movement::release_all() {
  for (double& t : press_time_) t = -1;
  ax_ = ay_ = 0;
}

bool Movement::idle() const {
  for (double t : press_time_)
    if (t >= 0) return false;
  return true;
}

Delta Movement::update(double now, double dt) {
  double speed_x = 0, speed_y = 0;
  bool any = false;
  for (int i = 0; i < 4; i++) {
    if (press_time_[i] < 0) continue;
    any = true;
    double hold = now - press_time_[i];
    // scale acceleration and max speed per axis based on screen layout
    double sx = kVecX[i] != 0 ? sx_ : 1;
    double sy = kVecY[i] != 0 ? sy_ : 1;
    double spx = std::min(initial_ + accel_ * sx * hold, max_ * sx);
    double spy = std::min(initial_ + accel_ * sy * hold, max_ * sy);
    speed_x += kVecX[i] * spx;
    speed_y += kVecY[i] * spy;
  }

  if (!any) {
    ax_ = ay_ = 0;
    return {};
  }

  ax_ += speed_x * dt;
  ay_ += speed_y * dt;

  Delta d;
  d.dx = static_cast<int>(std::trunc(ax_));
  d.dy = static_cast<int>(std::trunc(ay_));
  ax_ -= d.dx;
  ay_ -= d.dy;
  return d;
}

}  // namespace mk
