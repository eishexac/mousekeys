#pragma once
#include <functional>
#include <string>
#include <vector>

#include "core/click.h"
#include "core/config.h"
#include "core/movement.h"
#include "core/scroll.h"

namespace mk {

// Keycodes are the backend's native codes; the core never interprets them.
// 0 is a real keycode on macOS, so the unbound sentinel is all-ones.
constexpr unsigned kNoKey = 0xffffffffu;

// Modifier bitmask reported by backends alongside each key event.
enum Mods : unsigned { kModCtrl = 1, kModAlt = 2, kModCmd = 4, kModFn = 8 };

// Typed settings with the spoon's defaults, filled from an INI Config.
struct Settings {
  // [layer]
  std::string layer_key = "capslock";
  double tap_timeout = 0.2;
  // [movement]
  double initial_speed = 100, acceleration = 5000, max_speed = 4000;
  double interval = 0.016;
  double scale_x = 0, scale_y = 0;  // 0 = backend decides (auto)
  // [scroll]
  double scroll_initial_speed = 20, scroll_acceleration = 800, scroll_max_speed = 600;
  double scroll_friction = 0.85, scroll_tap = 1, scroll_pixel_scale = 4;
  // [keys] — [0] primary, [1] secondary ("" = unbound)
  std::string up[2] = {"i", ""}, down[2] = {"k", ""};
  std::string left[2] = {"j", ""}, right[2] = {"l", ""};
  std::string left_click[2] = {"u", ""}, right_click[2] = {"o", ""};
  std::string scroll_up[2] = {";", ""}, scroll_down[2] = {"/", ""};
  std::string scroll_left[2] = {"'", ""}, scroll_right[2] = {"\\", ""};
  // [macos]
  bool alerts = true;                    // on-screen HUD for reload / unbound keys
  std::string alert_position = "center";  // top | center | bottom

  static Settings from_config(const Config& c);
};

// "" on success, a message on nonsense values.
std::string validate(const Settings& s);

// Loads path ("" = pure defaults) into *out. False with err set on an
// unreadable file, parse error, malformed number, or a rejected value.
bool load_settings(const std::string& path, Settings* out, std::string* err);

// Merges several config files in order (later keys win) into one Settings.
// An empty list yields pure defaults. Same failure modes as load_settings.
bool load_settings_files(const std::vector<std::string>& files, Settings* out,
                         std::string* err);

// Convenience: load_settings_files(config_files(dir), ...).
bool load_settings_dir(const std::string& dir, Settings* out, std::string* err);

// Key bindings resolved to the backend's native keycodes.
struct Bindings {
  unsigned layer_key = kNoKey;
  unsigned move_keys[4][2];    // [Dir][primary/secondary]
  unsigned scroll_keys[4][2];  // [Dir][primary/secondary]
  unsigned left_click[2], right_click[2];
  unsigned panic[3];  // left shift, right shift, escape — set by the backend
  Bindings();
};

// Resolves every key name in st through the backend's resolver, which
// returns kNoKey for names it does not know.
bool make_bindings(const Settings& st,
                   const std::function<unsigned(const std::string&)>& resolve,
                   Bindings* out, std::string* err);

// Everything the engine does lands here; backends implement it.
struct Output {
  virtual void move(int dx, int dy, bool dragging) = 0;
  virtual void left_button(bool down, int click_count) = 0;
  virtual void right_click() = 0;
  virtual void scroll(int dx, int dy) = 0;  // +dy = up, +dx = right, pixels
  // A non-modifier key was swallowed while mouse mode is active but bound
  // to nothing — surfaced so the user can tell the layer is on.
  virtual void unbound_key(unsigned code) = 0;
  virtual ~Output() = default;
};

// The layer state machine (QMK-style momentary hold + tap-toggle) with the
// movement/scroll/click modules behind it. Platform-free: backends feed
// key events and a clock in; actions come out through Output.
class Engine {
 public:
  Engine(const Settings& st, const Bindings& b, Output& out);

  enum class Verdict { Consume, Pass };

  // is_modifier: a bare modifier key press/release (shift/ctrl/alt/cmd);
  // those pass through and never mark the layer key as "used".
  Verdict on_key(unsigned code, bool down, unsigned mods, bool is_modifier,
                 double now);

  void tick(double now, double dt);
  bool wants_ticks() const;
  bool layer_active() const { return active_; }
  // True when mouse mode is latched on by a tap (sticky), as opposed to
  // held momentarily — lets the UI show a distinct "locked" indicator.
  bool layer_toggled() const { return toggled_; }
  // True once the panic chord (both shifts + escape) fired; the backend
  // must ungrab and exit. Checked before all other key handling so no
  // engine state can mask it.
  bool panic() const { return panic_; }
  void set_scale(double sx, double sy);
  // Releases everything and swaps new settings/bindings in (config reload).
  void reconfigure(const Settings& st, const Bindings& b);
  // Releases everything (process shutdown).
  void shutdown();
  const Settings& settings() const { return st_; }

 private:
  void apply_settings();
  void deactivate();
  static bool match(const unsigned b[2], unsigned code);

  Settings st_;
  Bindings b_;
  Output& out_;
  Movement move_;
  Scroll scroll_;
  Click click_;
  double scale_x_ = 1, scale_y_ = 1;
  bool active_ = false, toggled_ = false, used_ = false, panic_ = false;
  double down_time_ = -1;
  bool panic_held_[2] = {false, false};
};

}  // namespace mk
