#include "core/scroll.h"

#include <algorithm>
#include <cmath>

namespace mk {

// Scroll direction vectors: positive y scrolls up (matches the original
// spoon's hs.eventtap.scrollWheel convention).
static const double kVecX[4] = {0, 0, -1, 1};
static const double kVecY[4] = {1, -1, 0, 0};

void Scroll::configure(double initial_speed, double acceleration, double max_speed,
                       double friction, double tap, double pixel_scale) {
  initial_ = initial_speed;
  accel_ = acceleration;
  max_ = max_speed;
  friction_ = friction;
  tap_ = tap;
  pixel_scale_ = pixel_scale;
}

void Scroll::press(Dir d, double now) {
  int i = static_cast<int>(d);
  if (press_time_[i] < 0) press_time_[i] = now;
}

void Scroll::release(Dir d) { press_time_[static_cast<int>(d)] = -1; }

void Scroll::release_all() {
  for (double& t : press_time_) t = -1;
}

void Scroll::reset() {
  release_all();
  vx_ = vy_ = 0;
  ax_ = ay_ = 0;
  tap_fired_x_ = tap_fired_y_ = false;
}

bool Scroll::idle() const {
  for (double t : press_time_)
    if (t >= 0) return false;
  return std::fabs(vx_) < 0.1 && std::fabs(vy_) < 0.1;
}

Delta Scroll::update(double now, double dt) {
  bool any_y = press_time_[0] >= 0 || press_time_[1] >= 0;
  bool any_x = press_time_[2] >= 0 || press_time_[3] >= 0;

  // While holding: speed from hold duration (time-based, no friction).
  // On release: coast on the last velocity with friction decay.
  if (any_x || any_y) {
    double sx = 0, sy = 0;
    for (int i = 0; i < 4; i++) {
      if (press_time_[i] < 0) continue;
      double hold = now - press_time_[i];
      double speed = std::min(initial_ + accel_ * hold, max_);
      sx += kVecX[i] * speed;
      sy += kVecY[i] * speed;
    }
    vx_ = sx;
    vy_ = sy;
  } else {
    double f = std::pow(friction_, dt / 0.016);
    vx_ *= f;
    vy_ *= f;
  }

  // Tap: instant scroll on the first press of an idle axis.
  int out_x = 0, out_y = 0;
  if (!tap_fired_y_ && any_y) {
    tap_fired_y_ = true;
    int tap = static_cast<int>(tap_ * pixel_scale_);
    out_y = press_time_[0] >= 0 ? tap : -tap;
  }
  if (!tap_fired_x_ && any_x) {
    tap_fired_x_ = true;
    int tap = static_cast<int>(tap_ * pixel_scale_);
    out_x = press_time_[3] >= 0 ? tap : -tap;
  }

  ax_ += vx_ * dt * pixel_scale_;
  ay_ += vy_ * dt * pixel_scale_;
  int dx = static_cast<int>(std::trunc(ax_));
  int dy = static_cast<int>(std::trunc(ay_));
  ax_ -= dx;
  ay_ -= dy;
  out_x += dx;
  out_y += dy;

  // Reset tap state once an axis is fully idle.
  if (!any_y && std::fabs(vy_) < 0.1) {
    vy_ = 0;
    ay_ = 0;
    tap_fired_y_ = false;
  }
  if (!any_x && std::fabs(vx_) < 0.1) {
    vx_ = 0;
    ax_ = 0;
    tap_fired_x_ = false;
  }

  return {out_x, out_y};
}

}  // namespace mk
