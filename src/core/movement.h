#pragma once

namespace mk {

enum class Dir { Up = 0, Down = 1, Left = 2, Right = 3 };

struct Delta {
  int dx = 0;
  int dy = 0;
};

// Time-based cursor movement with no momentum: speed each frame is a
// function of how long the direction key has been held, so releasing a key
// stops that axis instantly and direction changes take effect immediately.
// Sub-pixel remainders accumulate across frames so slow speeds still move.
//
// Acceleration and max speed scale per axis (set_scale) so cursor feel
// stays consistent across screen layouts; the backend decides the factors.
class Movement {
public:
  void configure(double initial_speed, double acceleration, double max_speed);
  void set_scale(double sx, double sy);
  void press(Dir d, double now);
  void release(Dir d);
  void release_all();
  bool idle() const;
  Delta update(double now, double dt);

private:
  double initial_ = 100, accel_ = 5000, max_ = 4000;
  double sx_ = 1, sy_ = 1;
  double press_time_[4] = {-1, -1, -1, -1};  // indexed by Dir; -1 = not held
  double ax_ = 0, ay_ = 0;                   // sub-pixel accumulators
};

}  // namespace mk
