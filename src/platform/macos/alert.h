#pragma once
#include <string>

// Minimal on-screen HUD, hs.alert-style: a rounded translucent panel
// centered on the active screen that fades out after a moment. Used for
// config-reload feedback and for unbound-key presses while mouse mode is
// active, so a swallowed key never looks like a dead keyboard. Implemented
// in alert.mm against AppKit; declared here so the C++ backend can call it
// without importing Objective-C headers.
namespace mk {
void show_alert(const std::string& text, double seconds);
// Vertical placement of the HUD: "top", "center", or "bottom" (default).
void set_alert_position(const std::string& pos);
}
