#include "core/default_config.h"

namespace mk {

const char* default_config_text() {
  return
R"CFG(# mousekeys configuration
#
# Lives at ~/.config/mousekeys/config (honors $XDG_CONFIG_HOME). Edit and
# save — the daemon reloads automatically, no restart. Lines starting with
# '#' are comments. Regenerate this file any time with:
#     mousekeysd --print-default-config
#
# Key bindings are comma lists: "primary[, secondary]". Section and key
# names are case-insensitive.

[layer]
# Physical key that activates mouse mode. While running it is remapped to a
# spare key and restored on quit; Caps Lock is the usual choice on macOS.
key = capslock
# Longest hold (seconds) still counted as a tap. Tap = latch mouse mode on
# or off; hold = momentary, active only while the key is held.
tap_timeout = 0.2

[movement]
initial_speed = 100     # px/sec on the first frame (controls tap precision)
acceleration  = 5000    # px/sec^2 ramp while a direction key is held
max_speed     = 4000    # px/sec cap
interval      = 0.016   # timer period, ~60 fps
# Fixed per-axis speed scaling. Leave commented for automatic scaling from
# the screen layout (recommended on macOS; ignored where there is no
# display server, e.g. a headless daemon).
# scale_x = 1
# scale_y = 1
# Direction keys.
up    = i
down  = k
left  = j
right = l

[click]
left  = u
right = o

[scroll]
initial_speed = 20
acceleration  = 800
max_speed     = 600
friction      = 0.85    # glide decay after release (0..1, higher = longer)
tap           = 1       # instant scroll amount on first press
pixel_scale   = 4
# Direction keys.
up    = ;
down  = /
left  = '
right = \

[macos]
# On-screen alerts for config reload and for keys that do nothing while
# mouse mode is active. The menu-bar icon always reflects state regardless.
alerts = on
# Where the alert appears. Vertical and/or horizontal:
#   center | top | bottom | left | right |
#   top_left | top_right | bottom_left | bottom_right
alert_position = center
)CFG";
}

}  // namespace mk
