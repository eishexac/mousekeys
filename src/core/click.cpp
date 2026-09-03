#include "core/click.h"

namespace mk {

int Click::left_down(double now) {
  if (held_) return 0;
  held_ = true;
  count_ = (now - last_) < interval_ ? count_ + 1 : 1;
  last_ = now;
  return count_;
}

int Click::left_up() {
  if (!held_) return 0;
  held_ = false;
  return count_;
}

}  // namespace mk
