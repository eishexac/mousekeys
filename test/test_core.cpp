// Core unit tests: plain asserts, no framework, a fake clock (plain
// doubles) and a recording Output. `make check` builds and runs this on
// any platform with a C++17 compiler — no macOS or Linux APIs involved.
#include <sys/stat.h>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "core/click.h"
#include "core/config.h"
#include "core/default_config.h"
#include "core/engine.h"
#include "core/movement.h"
#include "core/scroll.h"

using namespace mk;

namespace {

struct Rec : Output {
  struct Ev {
    std::string kind;
    int a = 0, b = 0;
    bool flag = false;
  };
  std::vector<Ev> evs;
  void move(int dx, int dy, bool drag) override { evs.push_back({"move", dx, dy, drag}); }
  void left_button(bool down, int n) override {
    evs.push_back({down ? "ldown" : "lup", n, 0, false});
  }
  void right_click() override { evs.push_back({"rclick", 0, 0, false}); }
  void scroll(int dx, int dy) override { evs.push_back({"scroll", dx, dy, false}); }
  void unbound_key(unsigned code) override {
    evs.push_back({"unbound", static_cast<int>(code), 0, false});
  }
  int count(const std::string& kind) const {
    int n = 0;
    for (auto& e : evs)
      if (e.kind == kind) n++;
    return n;
  }
  const Ev* last(const std::string& kind) const {
    for (auto it = evs.rbegin(); it != evs.rend(); ++it)
      if (it->kind == kind) return &*it;
    return nullptr;
  }
};

// Arbitrary keycodes; the engine treats them as opaque.
enum : unsigned {
  LAYER = 100,
  KUP = 1, KDOWN = 2, KLEFT = 3, KRIGHT = 4,
  LCLICK = 5, RCLICK = 6,
  SUP = 7, SDOWN = 8, SLEFT = 9, SRIGHT = 10,
  KUP2 = 11,
  UNBOUND = 50, MODKEY = 60,
  PSL = 20, PSR = 21, PESC = 22,
};

Bindings test_bindings() {
  Bindings b;
  b.layer_key = LAYER;
  b.move_keys[(int)Dir::Up][0] = KUP;
  b.move_keys[(int)Dir::Up][1] = KUP2;
  b.move_keys[(int)Dir::Down][0] = KDOWN;
  b.move_keys[(int)Dir::Left][0] = KLEFT;
  b.move_keys[(int)Dir::Right][0] = KRIGHT;
  b.left_click[0] = LCLICK;
  b.right_click[0] = RCLICK;
  b.scroll_keys[(int)Dir::Up][0] = SUP;
  b.scroll_keys[(int)Dir::Down][0] = SDOWN;
  b.scroll_keys[(int)Dir::Left][0] = SLEFT;
  b.scroll_keys[(int)Dir::Right][0] = SRIGHT;
  b.panic[0] = PSL;
  b.panic[1] = PSR;
  b.panic[2] = PESC;
  return b;
}

using V = Engine::Verdict;

// ---- movement ----

void movement_ramp() {
  Movement m;
  m.configure(100, 5000, 4000);
  m.press(Dir::Up, 0);
  Delta d = m.update(0.1, 0.1);  // speed = 100 + 5000*0.1 = 600 px/s
  assert(d.dx == 0);
  assert(d.dy == -60);
}

void movement_subpixel_accumulation() {
  Movement m;
  m.configure(100, 5000, 4000);
  m.press(Dir::Up, 0);
  Delta d1 = m.update(0.005, 0.005);  // 0.625 px — below one pixel
  assert(d1.dx == 0 && d1.dy == 0);
  Delta d2 = m.update(0.010, 0.005);  // accumulated 1.375 px
  assert(d2.dy == -1);
}

void movement_instant_stop() {
  Movement m;
  m.configure(100, 5000, 4000);
  m.press(Dir::Up, 0);
  m.update(0.1, 0.1);
  m.release(Dir::Up);
  assert(m.idle());
  Delta d = m.update(0.2, 0.1);
  assert(d.dx == 0 && d.dy == 0);
}

void movement_max_speed_clamp() {
  Movement m;
  m.configure(100, 5000, 4000);
  m.press(Dir::Right, 0);
  Delta d = m.update(10, 0.016);  // long hold: clamped to 4000 px/s
  assert(d.dx == 64);
}

void movement_axis_scale() {
  Movement m;
  m.configure(100, 5000, 4000);
  m.set_scale(2, 1);
  m.press(Dir::Right, 0);
  Delta d = m.update(1, 1);  // clamp = max_speed * scale_x = 8000
  assert(d.dx == 8000);
}

void movement_diagonal() {
  Movement m;
  m.configure(100, 5000, 4000);
  m.press(Dir::Up, 0);
  m.press(Dir::Right, 0);
  Delta d = m.update(0.1, 0.1);
  assert(d.dx == 60 && d.dy == -60);
}

// ---- scroll ----

void scroll_tap_and_ramp() {
  Scroll s;
  s.configure(20, 800, 600, 0.85, 1, 4);
  s.press(Dir::Up, 0);
  Delta d = s.update(0.016, 0.016);
  // tap: 1*4 = 4, plus ramp: speed 32.8 * 0.016s * 4 = 2.09 px -> 2
  assert(d.dx == 0);
  assert(d.dy == 6);
}

void scroll_tap_direction() {
  Scroll s;
  s.configure(20, 800, 600, 0.85, 1, 4);
  s.press(Dir::Down, 0);
  Delta d = s.update(0.016, 0.016);
  assert(d.dy == -6);
}

void scroll_glide_decays() {
  Scroll s;
  s.configure(20, 800, 600, 0.85, 1, 4);
  s.press(Dir::Up, 0);
  s.update(0.5, 0.016);  // build up velocity
  s.release(Dir::Up);
  int total = 0;
  double t = 0.5;
  bool went_idle = false;
  for (int i = 0; i < 200; i++) {
    t += 0.016;
    total += s.update(t, 0.016).dy;
    if (s.idle()) {
      went_idle = true;
      break;
    }
  }
  assert(total > 0);      // glide scrolled some more after release
  assert(went_idle);      // and friction actually stopped it
}

void scroll_reset_hard_stops() {
  Scroll s;
  s.configure(20, 800, 600, 0.85, 1, 4);
  s.press(Dir::Up, 0);
  s.update(0.5, 0.016);
  s.reset();
  assert(s.idle());
  Delta d = s.update(0.6, 0.016);
  assert(d.dx == 0 && d.dy == 0);
}

// ---- click ----

void click_multi_detection() {
  Click c;
  assert(c.left_down(0.0) == 1);
  assert(c.left_held());
  assert(c.left_up() == 1);
  assert(c.left_down(0.2) == 2);  // within the 0.3s window
  assert(c.left_up() == 2);
  assert(c.left_down(0.6) == 1);  // window expired
  assert(c.left_down(0.7) == 0);  // already held
  assert(c.left_up() == 1);
}

// ---- engine: layer state machine ----

void engine_momentary() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);

  assert(e.on_key(LAYER, true, 0, false, 0.0) == V::Consume);
  assert(e.layer_active());
  assert(e.on_key(KUP, true, 0, false, 0.05) == V::Consume);
  assert(e.wants_ticks());
  e.tick(0.066, 0.016);
  const Rec::Ev* mv = r.last("move");
  assert(mv && mv->b < 0 && !mv->flag);

  // held past tapTimeout -> release deactivates
  assert(e.on_key(LAYER, false, 0, false, 0.5) == V::Consume);
  assert(!e.layer_active());
  assert(!e.wants_ticks());  // movement released with the layer
  assert(e.on_key(UNBOUND, true, 0, false, 1.0) == V::Pass);
}

void engine_tap_toggle() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);

  e.on_key(LAYER, true, 0, false, 0.0);
  e.on_key(LAYER, false, 0, false, 0.1);  // tap: toggle on
  assert(e.layer_active());
  assert(e.on_key(UNBOUND, true, 0, false, 0.2) == V::Consume);
  assert(r.count("unbound") == 1);
  assert(e.on_key(UNBOUND, false, 0, false, 0.25) == V::Consume);
  assert(r.count("unbound") == 1);  // only key-down alerts

  e.on_key(LAYER, true, 0, false, 1.0);
  e.on_key(LAYER, false, 0, false, 1.05);  // tap again: toggle off
  assert(!e.layer_active());
  assert(e.on_key(UNBOUND, true, 0, false, 1.1) == V::Pass);
}

void engine_used_hold_is_momentary() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);

  e.on_key(LAYER, true, 0, false, 0.0);
  e.on_key(KUP, true, 0, false, 0.05);
  e.on_key(KUP, false, 0, false, 0.08);
  // released within tapTimeout, but a key was used -> not a toggle
  e.on_key(LAYER, false, 0, false, 0.1);
  assert(!e.layer_active());
}

void engine_click_and_drag() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);

  e.on_key(LAYER, true, 0, false, 0.0);
  e.on_key(LAYER, false, 0, false, 0.1);  // toggle on

  e.on_key(LCLICK, true, 0, false, 0.5);
  const Rec::Ev* dn = r.last("ldown");
  assert(dn && dn->a == 1);
  e.on_key(KRIGHT, true, 0, false, 0.55);
  e.tick(0.6, 0.016);
  const Rec::Ev* mv = r.last("move");
  assert(mv && mv->flag);  // dragging while left button is held
  e.on_key(LCLICK, false, 0, false, 0.7);
  const Rec::Ev* up = r.last("lup");
  assert(up && up->a == 1);
  e.on_key(KRIGHT, false, 0, false, 0.7);

  // second press inside the double-click window
  e.on_key(LCLICK, true, 0, false, 0.72);
  dn = r.last("ldown");
  assert(dn && dn->a == 2);
  e.on_key(LCLICK, false, 0, false, 0.75);
}

void engine_toggle_exit_releases_click() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);

  e.on_key(LAYER, true, 0, false, 0.0);
  e.on_key(LAYER, false, 0, false, 0.1);  // toggle on
  e.on_key(LCLICK, true, 0, false, 1.0);
  assert(r.count("lup") == 0);
  e.on_key(LAYER, true, 0, false, 2.0);
  e.on_key(LAYER, false, 0, false, 2.05);  // tap: toggle off
  assert(!e.layer_active());
  const Rec::Ev* up = r.last("lup");
  assert(up && up->a == 1);  // held button was released on exit
}

void engine_right_click() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);
  e.on_key(LAYER, true, 0, false, 0.0);
  e.on_key(LAYER, false, 0, false, 0.1);
  e.on_key(RCLICK, true, 0, false, 0.5);
  assert(r.count("rclick") == 1);
  e.on_key(RCLICK, false, 0, false, 0.6);
  assert(r.count("rclick") == 1);  // fires on key-down only
}

void engine_scroll_keys() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);
  e.on_key(LAYER, true, 0, false, 0.0);
  e.on_key(LAYER, false, 0, false, 0.1);
  assert(e.on_key(SUP, true, 0, false, 0.5) == V::Consume);
  assert(e.wants_ticks());
  e.tick(0.516, 0.016);
  const Rec::Ev* sc = r.last("scroll");
  assert(sc && sc->b > 0);
  e.on_key(SUP, false, 0, false, 0.6);
}

void engine_modifier_combos_pass() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);
  e.on_key(LAYER, true, 0, false, 0.0);
  e.on_key(LAYER, false, 0, false, 0.1);  // toggle on

  // Cmd+key passes through untouched (e.g. Cmd+Tab)
  assert(e.on_key(UNBOUND, true, kModCmd, false, 0.5) == V::Pass);
  assert(r.count("unbound") == 0);
  // ...but a bound direction key is consumed even with a modifier held
  // (spoon parity: binding checks run before the modifier check)
  assert(e.on_key(KUP, true, kModCmd, false, 0.6) == V::Consume);
  e.on_key(KUP, false, 0, false, 0.65);
  // bare modifiers pass through
  assert(e.on_key(MODKEY, true, 0, true, 0.7) == V::Pass);
}

void engine_modifier_does_not_mark_used() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);
  e.on_key(LAYER, true, 0, false, 0.0);
  e.on_key(MODKEY, true, 0, true, 0.05);   // bare modifier during hold
  e.on_key(MODKEY, false, 0, true, 0.08);
  e.on_key(LAYER, false, 0, false, 0.1);   // still counts as a tap
  assert(e.layer_active());                // -> toggled on
}

void engine_secondary_binding() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);
  e.on_key(LAYER, true, 0, false, 0.0);
  assert(e.on_key(KUP2, true, 0, false, 0.05) == V::Consume);
  assert(e.wants_ticks());
  e.on_key(KUP2, false, 0, false, 0.1);
}

void engine_panic_chord() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);
  assert(!e.panic());
  e.on_key(PSL, true, 0, true, 0.0);
  e.on_key(PSR, true, 0, true, 0.05);
  assert(!e.panic());
  e.on_key(PESC, true, 0, false, 0.1);
  assert(e.panic());
}

void engine_reconfigure_resets() {
  Rec r;
  Settings st;
  Engine e(st, test_bindings(), r);
  e.on_key(LAYER, true, 0, false, 0.0);
  e.on_key(LAYER, false, 0, false, 0.1);  // toggle on
  e.on_key(LCLICK, true, 0, false, 0.5);

  Settings st2;
  st2.initial_speed = 250;
  e.reconfigure(st2, test_bindings());
  assert(!e.layer_active());
  const Rec::Ev* up = r.last("lup");
  assert(up);  // held click was released by the swap
  assert(e.settings().initial_speed == 250);
}

// ---- config ----

void config_parse_and_lookup() {
  Config c;
  std::string err;
  bool ok = c.parse(
      "# top comment\n"
      "[movement]\n"
      "initial_speed = 250  # inline comment\n"
      "left = j\n"
      "[scroll]\n"
      "up = ;\n",
      err);
  assert(ok);
  assert(c.get_num("movement.initial_speed", 0) == 250);
  assert(c.get("scroll.up", "") == ";");
  assert(c.get("movement.left", "") == "j");
  assert(c.get("movement.missing", "fallback") == "fallback");
}

void config_bad_number_recorded() {
  Config c;
  std::string err;
  assert(c.parse("[movement]\nmax_speed = fast\n", err));
  assert(c.get_num("movement.max_speed", 4000) == 4000);
  assert(c.bad_keys().size() == 1);
  assert(c.bad_keys()[0] == "movement.max_speed");
}

void config_malformed_line_fails() {
  Config c;
  std::string err;
  assert(!c.parse("[movement]\nthis is not a key value pair\n", err));
  assert(!err.empty());
}

void settings_from_config_and_validate() {
  Config c;
  std::string err;
  assert(c.parse(
      "[layer]\nkey = right_control\ntap_timeout = 0.25\n"
      "[movement]\ninitial_speed = 150\nup = w, up\n"
      "[macos]\nalerts = off\n",
      err));
  Settings s = Settings::from_config(c);
  assert(s.layer_key == "right_control");
  assert(s.tap_timeout == 0.25);
  assert(s.initial_speed == 150);
  assert(s.acceleration == 5000);  // untouched default
  assert(s.up[0] == "w" && s.up[1] == "up");
  assert(s.down[0] == "k");  // untouched default
  assert(!s.alerts);
  assert(validate(s).empty());

  Settings bad;
  bad.scroll_friction = 1.5;
  assert(!validate(bad).empty());
}

void load_settings_defaults() {
  Settings s;
  std::string err;
  assert(load_settings("", &s, &err));
  assert(s.layer_key == "capslock");
  assert(!load_settings("/nonexistent/mousekeys.conf", &s, &err));
  assert(!err.empty());
}

// The shipped default config must itself parse and validate — otherwise a
// fresh install (seeded from it) would fail to start.
void default_config_is_valid() {
  Config c;
  std::string err;
  assert(c.parse(default_config_text(), err));
  assert(c.bad_keys().empty());
  Settings s = Settings::from_config(c);
  assert(validate(s).empty());
  assert(s.up[0] == "i");
  assert(s.scroll_right[0] == "\\");  // backslash survives parsing
}

// Case-insensitive sections/keys, comma bindings, and single-item clearing.
void config_case_and_comma() {
  Config c;
  std::string err;
  assert(c.parse(
      "[Movement]\nInitial_Speed = 250\nUp = W, Up\nLeft = a\n",
      err));
  Settings s = Settings::from_config(c);
  assert(s.initial_speed == 250);              // [Movement]/Initial_Speed match
  assert(s.up[0] == "w" && s.up[1] == "up");   // comma split + lowercased
  assert(s.left[0] == "a" && s.left[1] == "");  // one item clears the secondary
  assert(s.down[0] == "k");                    // untouched default kept
}

static void writef(const std::string& path, const std::string& body) {
  std::ofstream(path) << body;
}

// A base `config` plus sorted `config.d/*.conf`, later keys overriding.
void config_dir_merge_and_order() {
  std::string dir = "build/tcfg";
  mkdir("build", 0755);
  mkdir(dir.c_str(), 0755);
  mkdir((dir + "/config.d").c_str(), 0755);
  writef(dir + "/config", "[movement]\ninitial_speed = 111\nacceleration = 111\n");
  writef(dir + "/config.d/20-late.conf", "[movement]\ninitial_speed = 333\n");
  writef(dir + "/config.d/10-early.conf",
         "[movement]\ninitial_speed = 222\nacceleration = 999\n");
  writef(dir + "/config.d/skip.txt", "[movement]\ninitial_speed = 7\n");  // not .conf

  auto files = config_files(dir);
  assert(files.size() == 3);
  assert(files[0] == dir + "/config");
  assert(files[1] == dir + "/config.d/10-early.conf");  // sorted before 20-
  assert(files[2] == dir + "/config.d/20-late.conf");

  Settings s;
  std::string err;
  assert(load_settings_dir(dir, &s, &err));
  assert(s.initial_speed == 333);  // 20-late wins over 10-early and base
  assert(s.acceleration == 999);   // 10-early wins over base; not overridden
}

void config_files_empty_when_absent() {
  assert(config_files("/nonexistent/mousekeys").empty());
  assert(config_files("").empty());
  Settings s;
  std::string err;
  assert(load_settings_dir("/nonexistent/mousekeys", &s, &err));  // -> defaults
  assert(s.layer_key == "capslock");
}

void make_bindings_resolves() {
  Settings s;  // defaults: i/k/j/l etc.
  auto resolve = [](const std::string& name) -> unsigned {
    // toy resolver: single lowercase letters and a few punctuation names
    if (name.size() == 1) return static_cast<unsigned>(name[0]);
    if (name == "capslock") return 1000;
    return kNoKey;
  };
  Bindings b;
  std::string err;
  assert(make_bindings(s, resolve, &b, &err));
  assert(b.layer_key == 1000);
  assert(b.move_keys[(int)Dir::Up][0] == 'i');
  assert(b.move_keys[(int)Dir::Up][1] == kNoKey);
  assert(b.scroll_keys[(int)Dir::Up][0] == ';');

  Settings s2;
  s2.up[1] = "no_such_key";
  assert(!make_bindings(s2, resolve, &b, &err));
  assert(err.find("no_such_key") != std::string::npos);
}

}  // namespace

#define RUN(fn)                 \
  do {                          \
    fn();                       \
    printf("ok %s\n", #fn);     \
    tests++;                    \
  } while (0)

int main() {
  int tests = 0;
  RUN(movement_ramp);
  RUN(movement_subpixel_accumulation);
  RUN(movement_instant_stop);
  RUN(movement_max_speed_clamp);
  RUN(movement_axis_scale);
  RUN(movement_diagonal);
  RUN(scroll_tap_and_ramp);
  RUN(scroll_tap_direction);
  RUN(scroll_glide_decays);
  RUN(scroll_reset_hard_stops);
  RUN(click_multi_detection);
  RUN(engine_momentary);
  RUN(engine_tap_toggle);
  RUN(engine_used_hold_is_momentary);
  RUN(engine_click_and_drag);
  RUN(engine_toggle_exit_releases_click);
  RUN(engine_right_click);
  RUN(engine_scroll_keys);
  RUN(engine_modifier_combos_pass);
  RUN(engine_modifier_does_not_mark_used);
  RUN(engine_secondary_binding);
  RUN(engine_panic_chord);
  RUN(engine_reconfigure_resets);
  RUN(config_parse_and_lookup);
  RUN(config_bad_number_recorded);
  RUN(config_malformed_line_fails);
  RUN(settings_from_config_and_validate);
  RUN(config_case_and_comma);
  RUN(load_settings_defaults);
  RUN(default_config_is_valid);
  RUN(config_dir_merge_and_order);
  RUN(config_files_empty_when_absent);
  RUN(make_bindings_resolves);
  printf("%d tests passed\n", tests);
  return 0;
}
