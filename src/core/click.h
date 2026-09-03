#pragma once

namespace mk {

// Left-button state with multi-click detection. The engine calls left_down
// on key press and left_up on release; the returned click count (1 =
// single, 2 = double, ...) is attached to the injected button events so
// double/triple clicks work. Right click needs no state and lives in the
// engine directly.
class Click {
public:
  // Returns the click count for the press, or 0 if the button was already
  // held (no event should be injected).
  int left_down(double now);
  // Returns the click count for the release, or 0 if the button was not
  // held.
  int left_up();
  bool left_held() const { return held_; }

private:
  bool held_ = false;
  double last_ = -1e9;
  int count_ = 0;
  double interval_ = 0.3;  // double-click window, matches click.lua
};

}  // namespace mk
