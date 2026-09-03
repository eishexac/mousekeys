#include "core/engine.h"

namespace mk {

Bindings::Bindings() {
  for (int d = 0; d < 4; d++)
    for (int i = 0; i < 2; i++) move_keys[d][i] = scroll_keys[d][i] = kNoKey;
  left_click[0] = left_click[1] = kNoKey;
  right_click[0] = right_click[1] = kNoKey;
  panic[0] = panic[1] = panic[2] = kNoKey;
}

static std::string lower(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

// A binding value is a comma list: "primary[, secondary]". Absent key keeps
// the default; a present key with one item clears the secondary. Key names
// are lowercased so "W"/"Return"/"F18" resolve like their canonical forms.
// `full` is the section-qualified key, e.g. "movement.up".
static void get_binding(const Config& c, const std::string& full, std::string out[2]) {
  if (!c.has(full)) return;
  auto list = c.get_list(full);
  out[0] = list.size() >= 1 ? lower(list[0]) : "";
  out[1] = list.size() >= 2 ? lower(list[1]) : "";
}

Settings Settings::from_config(const Config& c) {
  Settings s;
  s.layer_key = lower(c.get("layer.key", s.layer_key));
  s.tap_timeout = c.get_num("layer.tap_timeout", s.tap_timeout);

  s.initial_speed = c.get_num("movement.initial_speed", s.initial_speed);
  s.acceleration = c.get_num("movement.acceleration", s.acceleration);
  s.max_speed = c.get_num("movement.max_speed", s.max_speed);
  s.interval = c.get_num("movement.interval", s.interval);
  s.scale_x = c.get_num("movement.scale_x", s.scale_x);
  s.scale_y = c.get_num("movement.scale_y", s.scale_y);

  s.scroll_initial_speed = c.get_num("scroll.initial_speed", s.scroll_initial_speed);
  s.scroll_acceleration = c.get_num("scroll.acceleration", s.scroll_acceleration);
  s.scroll_max_speed = c.get_num("scroll.max_speed", s.scroll_max_speed);
  s.scroll_friction = c.get_num("scroll.friction", s.scroll_friction);
  s.scroll_tap = c.get_num("scroll.tap", s.scroll_tap);
  s.scroll_pixel_scale = c.get_num("scroll.pixel_scale", s.scroll_pixel_scale);

  // Direction keys live in their own sections alongside the tuning params.
  get_binding(c, "movement.up", s.up);
  get_binding(c, "movement.down", s.down);
  get_binding(c, "movement.left", s.left);
  get_binding(c, "movement.right", s.right);
  get_binding(c, "click.left", s.left_click);
  get_binding(c, "click.right", s.right_click);
  get_binding(c, "scroll.up", s.scroll_up);
  get_binding(c, "scroll.down", s.scroll_down);
  get_binding(c, "scroll.left", s.scroll_left);
  get_binding(c, "scroll.right", s.scroll_right);

  std::string a = lower(c.get("macos.alerts", s.alerts ? "on" : "off"));
  s.alerts = !(a == "off" || a == "false" || a == "0" || a == "no");
  s.alert_position = lower(c.get("macos.alert_position", s.alert_position));
  return s;
}

std::string validate(const Settings& s) {
  if (s.layer_key.empty()) return "layer.key must be set";
  if (!(s.tap_timeout > 0)) return "layer.tap_timeout must be > 0";
  if (!(s.interval >= 0.001 && s.interval <= 0.5))
    return "movement.interval out of range (0.001-0.5 seconds)";
  if (!(s.initial_speed >= 0 && s.acceleration >= 0 && s.max_speed > 0))
    return "movement speeds must be positive";
  if (!(s.scroll_friction >= 0 && s.scroll_friction < 1))
    return "scroll.friction must be in [0, 1)";
  if (!(s.scroll_pixel_scale > 0)) return "scroll.pixel_scale must be > 0";
  return "";
}

bool load_settings_files(const std::vector<std::string>& files, Settings* out,
                         std::string* err) {
  Config c;
  for (const auto& f : files)
    if (!c.parse_file(f, *err)) return false;  // later files override earlier
  Settings s = Settings::from_config(c);
  if (!c.bad_keys().empty()) {
    *err = "not a number: " + c.bad_keys().front();
    return false;
  }
  std::string v = validate(s);
  if (!v.empty()) {
    *err = v;
    return false;
  }
  *out = s;
  return true;
}

bool load_settings_dir(const std::string& dir, Settings* out, std::string* err) {
  return load_settings_files(config_files(dir), out, err);
}

bool load_settings(const std::string& path, Settings* out, std::string* err) {
  if (path.empty()) return load_settings_files({}, out, err);
  return load_settings_files({path}, out, err);
}

static bool resolve_pair(const std::function<unsigned(const std::string&)>& resolve,
                         const std::string name[2], unsigned out[2],
                         std::string* err) {
  for (int i = 0; i < 2; i++) {
    if (name[i].empty()) {
      out[i] = kNoKey;
      continue;
    }
    out[i] = resolve(name[i]);
    if (out[i] == kNoKey) {
      *err = "unknown key name '" + name[i] + "'";
      return false;
    }
  }
  return true;
}

bool make_bindings(const Settings& st,
                   const std::function<unsigned(const std::string&)>& resolve,
                   Bindings* out, std::string* err) {
  Bindings b;
  b.layer_key = resolve(st.layer_key);
  if (b.layer_key == kNoKey) {
    *err = "unknown key name '" + st.layer_key + "'";
    return false;
  }
  if (!resolve_pair(resolve, st.up, b.move_keys[(int)Dir::Up], err)) return false;
  if (!resolve_pair(resolve, st.down, b.move_keys[(int)Dir::Down], err)) return false;
  if (!resolve_pair(resolve, st.left, b.move_keys[(int)Dir::Left], err)) return false;
  if (!resolve_pair(resolve, st.right, b.move_keys[(int)Dir::Right], err)) return false;
  if (!resolve_pair(resolve, st.left_click, b.left_click, err)) return false;
  if (!resolve_pair(resolve, st.right_click, b.right_click, err)) return false;
  if (!resolve_pair(resolve, st.scroll_up, b.scroll_keys[(int)Dir::Up], err)) return false;
  if (!resolve_pair(resolve, st.scroll_down, b.scroll_keys[(int)Dir::Down], err)) return false;
  if (!resolve_pair(resolve, st.scroll_left, b.scroll_keys[(int)Dir::Left], err)) return false;
  if (!resolve_pair(resolve, st.scroll_right, b.scroll_keys[(int)Dir::Right], err)) return false;
  *out = b;
  return true;
}

Engine::Engine(const Settings& st, const Bindings& b, Output& out)
    : st_(st), b_(b), out_(out) {
  apply_settings();
}

void Engine::apply_settings() {
  move_.configure(st_.initial_speed, st_.acceleration, st_.max_speed);
  move_.set_scale(scale_x_, scale_y_);
  scroll_.configure(st_.scroll_initial_speed, st_.scroll_acceleration,
                    st_.scroll_max_speed, st_.scroll_friction, st_.scroll_tap,
                    st_.scroll_pixel_scale);
}

void Engine::set_scale(double sx, double sy) {
  scale_x_ = sx;
  scale_y_ = sy;
  move_.set_scale(sx, sy);
}

void Engine::reconfigure(const Settings& st, const Bindings& b) {
  shutdown();
  st_ = st;
  b_ = b;
  apply_settings();
}

void Engine::shutdown() {
  deactivate();
  toggled_ = false;
  down_time_ = -1;
  scroll_.reset();
}

void Engine::deactivate() {
  active_ = false;
  move_.release_all();
  scroll_.release_all();  // glide keeps decaying, as in the spoon
  int n = click_.left_up();
  if (n) out_.left_button(false, n);
}

bool Engine::match(const unsigned b[2], unsigned code) {
  return code == b[0] || code == b[1];
}

Engine::Verdict Engine::on_key(unsigned code, bool down, unsigned mods,
                               bool is_modifier, double now) {
  // Panic chord first: both shifts + escape must work no matter what state
  // the rest of the engine is in — on dom0 this is the only way back.
  if (code == b_.panic[0]) panic_held_[0] = down;
  if (code == b_.panic[1]) panic_held_[1] = down;
  if (code == b_.panic[2] && down && panic_held_[0] && panic_held_[1]) {
    panic_ = true;
    return Verdict::Pass;
  }

  if (code == b_.layer_key) {
    if (down) {
      if (down_time_ < 0) {  // ignore autorepeat
        down_time_ = now;
        used_ = false;
      }
      active_ = true;
    } else {
      double dur = down_time_ >= 0 ? now - down_time_ : 1e9;
      down_time_ = -1;
      if (dur < st_.tap_timeout && !used_) {
        toggled_ = !toggled_;
        if (toggled_)
          active_ = true;
        else
          deactivate();
      } else if (!toggled_) {
        deactivate();
      }
    }
    return Verdict::Consume;
  }

  if (!active_) return Verdict::Pass;

  // Any non-modifier key during the hold makes it strictly momentary.
  if (down && !is_modifier) used_ = true;

  for (int d = 0; d < 4; d++) {
    if (match(b_.move_keys[d], code)) {
      if (down)
        move_.press(static_cast<Dir>(d), now);
      else
        move_.release(static_cast<Dir>(d));
      return Verdict::Consume;
    }
  }

  if (match(b_.left_click, code)) {
    if (down) {
      int n = click_.left_down(now);
      if (n) out_.left_button(true, n);
    } else {
      int n = click_.left_up();
      if (n) out_.left_button(false, n);
    }
    return Verdict::Consume;
  }

  if (match(b_.right_click, code)) {
    if (down) out_.right_click();
    return Verdict::Consume;
  }

  for (int d = 0; d < 4; d++) {
    if (match(b_.scroll_keys[d], code)) {
      if (down)
        scroll_.press(static_cast<Dir>(d), now);
      else
        scroll_.release(static_cast<Dir>(d));
      return Verdict::Consume;
    }
  }

  // Modifier-only events and modifier+key combos pass through (Cmd+Tab,
  // Ctrl+C, ...). Everything else is swallowed while the layer is active —
  // and surfaced, so a swallowed key never looks like a dead keyboard.
  if (mods & (kModCtrl | kModAlt | kModCmd | kModFn)) return Verdict::Pass;
  if (is_modifier) return Verdict::Pass;
  if (down) out_.unbound_key(code);
  return Verdict::Consume;
}

void Engine::tick(double now, double dt) {
  Delta m = move_.update(now, dt);
  if (m.dx || m.dy) out_.move(m.dx, m.dy, click_.left_held());
  Delta s = scroll_.update(now, dt);
  if (s.dx || s.dy) out_.scroll(s.dx, s.dy);
}

bool Engine::wants_ticks() const { return !move_.idle() || !scroll_.idle(); }

}  // namespace mk
