#pragma once
#include "core/movement.h"  // Dir, Delta

namespace mk {

// Time-based scroll ramp while holding (like movement), momentum glide on
// release: the last held velocity decays through friction. First press on
// an idle axis also fires an instant "tap" scroll for precision.
//
// Sign convention: positive dy scrolls up, positive dx scrolls right.
// Output values are pixels, already multiplied by pixel_scale; backends
// convert to their native scroll units.
class Scroll {
public:
  void configure(double initial_speed, double acceleration, double max_speed,
                 double friction, double tap, double pixel_scale);
  void press(Dir d, double now);
  void release(Dir d);
  void release_all();  // clears held keys; glide continues to decay
  void reset();        // hard stop, glide included
  bool idle() const;
  Delta update(double now, double dt);

private:
  double initial_ = 20, accel_ = 800, max_ = 600;
  double friction_ = 0.85, tap_ = 1, pixel_scale_ = 4;
  double press_time_[4] = {-1, -1, -1, -1};  // indexed by Dir; -1 = not held
  double vx_ = 0, vy_ = 0;
  double ax_ = 0, ay_ = 0;
  bool tap_fired_x_ = false, tap_fired_y_ = false;
};

}  // namespace mk
