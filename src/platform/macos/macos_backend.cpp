// macOS backend: the Hammerspoon replacement. A CGEventTap intercepts
// keys, the mk::Engine decides, and CGEvents inject pointer motion, clicks
// and scroll. hidutil remaps the physical layer key to F18 so the layer key
// carries no OS behavior of its own (e.g. caps lock never toggles). A
// CFFileWatch on the config plus SIGHUP drive live reloads. Two on-screen
// alerts (config reload, unbound key in mouse mode) surface state the user
// would otherwise have to guess at.

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include "core/engine.h"
#include "platform/backend.h"
#include "platform/macos/alert.h"
#include "platform/macos/loginitem.h"
#include "platform/macos/menubar.h"

namespace mk {
namespace {

// ---- key names -> macOS virtual keycodes ----
// Letters/punctuation use the standard ANSI virtual keycodes; the named
// keys extend the set the spoon supported. hidutil remaps the physical
// layer key to F18, so "capslock" as a layer key resolves to F18's code.
std::map<std::string, unsigned> build_keymap() {
  return {
      {"a", 0}, {"s", 1}, {"d", 2}, {"f", 3}, {"h", 4}, {"g", 5}, {"z", 6},
      {"x", 7}, {"c", 8}, {"v", 9}, {"b", 11}, {"q", 12}, {"w", 13}, {"e", 14},
      {"r", 15}, {"y", 16}, {"t", 17}, {"1", 18}, {"2", 19}, {"3", 20}, {"4", 21},
      {"6", 22}, {"5", 23}, {"=", 24}, {"9", 25}, {"7", 26}, {"-", 27}, {"8", 28},
      {"0", 29}, {"]", 30}, {"o", 31}, {"u", 32}, {"[", 33}, {"i", 34}, {"p", 35},
      {"l", 37}, {"j", 38}, {"'", 39}, {"k", 40}, {";", 41}, {"\\", 42},
      {",", 43}, {"/", 44}, {"n", 45}, {"m", 46}, {".", 47}, {"`", 50},
      {"space", 49}, {"return", 36}, {"tab", 48}, {"escape", 53},
      {"delete", 51}, {"forwarddelete", 117},
      {"home", 115}, {"end", 119}, {"pageup", 116}, {"pagedown", 121},
      {"left", 123}, {"right", 124}, {"down", 125}, {"up", 126},
      {"f13", 105}, {"f14", 107}, {"f15", 113}, {"f16", 106},
      {"f17", 64}, {"f18", 79}, {"f19", 80}, {"f20", 90},
      // Layer key aliases: the physical key is remapped to F18 by hidutil,
      // so the engine watches F18's keycode regardless of which physical
      // key the user chose.
      {"capslock", 79}, {"section", 79},
      {"left_control", 59}, {"left_shift", 56}, {"left_option", 58},
      {"left_command", 55}, {"right_control", 62}, {"right_shift", 60},
      {"right_option", 61}, {"right_command", 54},
  };
}

// A human label for a virtual keycode, for the unbound-key alert: arrows and
// named keys as symbols/words, printable keys as their uppercased character,
// empty for anything we have no name for (caller falls back to a generic line).
std::string key_label(unsigned kc) {
  switch (kc) {
    case 49: return "Space";
    case 36: return "Return";
    case 48: return "Tab";
    case 53: return "Esc";
    case 51: return "Delete";
    case 117: return "Fwd Del";
    case 123: return "←";
    case 124: return "→";
    case 125: return "↓";
    case 126: return "↑";
    case 115: return "Home";
    case 119: return "End";
    case 116: return "Page Up";
    case 121: return "Page Down";
  }
  // Reverse the keymap, preferring the shortest name for a code (so 'a' beats a
  // long alias). Built once.
  static const std::map<unsigned, std::string> rev = [] {
    std::map<unsigned, std::string> r;
    for (auto& kv : build_keymap()) {
      auto it = r.find(kv.second);
      if (it == r.end() || kv.first.size() < it->second.size()) r[kv.second] = kv.first;
    }
    return r;
  }();
  auto it = rev.find(kc);
  if (it == rev.end()) return "";
  std::string s = it->second;
  for (auto& c : s)
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');  // a -> A, f13 -> F13
  return s;
}

// hidutil source codes for the physical key we remap to F18.
std::string hidutil_src(const std::string& name) {
  static const std::map<std::string, std::string> m = {
      {"capslock", "0x700000039"}, {"section", "0x700000064"},
      {"left_control", "0x7000000E0"}, {"left_shift", "0x7000000E1"},
      {"left_option", "0x7000000E2"}, {"left_command", "0x7000000E3"},
      {"right_control", "0x7000000E4"}, {"right_shift", "0x7000000E5"},
      {"right_option", "0x7000000E6"}, {"right_command", "0x7000000E7"},
      {"f18", "0x70000006D"}, {"f19", "0x70000006E"}, {"f20", "0x70000006F"},
  };
  auto it = m.find(name);
  return it == m.end() ? "" : it->second;
}
const char* kF18Dst = "0x70000006D";

// F18 is delivered as an ordinary keyDown/keyUp, so no flagsChanged dance
// (unlike the spoon, which had to handle a modifier layer key). Modifier
// virtual keycodes we still need to recognize as "bare modifier".
bool is_modifier_keycode(unsigned kc) {
  switch (kc) {
    case 54: case 55: case 56: case 58:
    case 59: case 60: case 61: case 62:
    case 57:  // caps lock (physical, pre-remap — shouldn't appear, but safe)
      return true;
    default:
      return false;
  }
}

unsigned flags_to_mods(CGEventFlags f) {
  unsigned m = 0;
  if (f & kCGEventFlagMaskControl) m |= kModCtrl;
  if (f & kCGEventFlagMaskAlternate) m |= kModAlt;
  if (f & kCGEventFlagMaskCommand) m |= kModCmd;
  if (f & kCGEventFlagMaskSecondaryFn) m |= kModFn;
  return m;
}

// ---- the output sink ----
class MacOutput : public Output {
 public:
  bool alerts = true;

  void move(int dx, int dy, bool dragging) override {
    CGPoint p = cursor();
    CGPoint np = clamp(CGPointMake(p.x + dx, p.y + dy));
    if (np.x == p.x && np.y == p.y) return;
    CGEventType t = dragging ? kCGEventLeftMouseDragged : kCGEventMouseMoved;
    CGEventRef e = CGEventCreateMouseEvent(nullptr, t, np,
                                           dragging ? kCGMouseButtonLeft : kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, e);
    CFRelease(e);
  }

  void left_button(bool down, int click_count) override {
    CGPoint p = cursor();
    CGEventRef e = CGEventCreateMouseEvent(
        nullptr, down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp, p,
        kCGMouseButtonLeft);
    CGEventSetIntegerValueField(e, kCGMouseEventClickState, click_count);
    CGEventPost(kCGHIDEventTap, e);
    CFRelease(e);
  }

  void right_click() override {
    CGPoint p = cursor();
    for (bool down : {true, false}) {
      CGEventRef e = CGEventCreateMouseEvent(
          nullptr, down ? kCGEventRightMouseDown : kCGEventRightMouseUp, p,
          kCGMouseButtonRight);
      CGEventPost(kCGHIDEventTap, e);
      CFRelease(e);
    }
  }

  void scroll(int dx, int dy) override {
    // +dy = up. CGScroll uses +y = up already, so pass through; +x = right.
    CGEventRef e = CGEventCreateScrollWheelEvent(
        nullptr, kCGScrollEventUnitPixel, 2, dy, dx);
    CGEventPost(kCGHIDEventTap, e);
    CFRelease(e);
  }

  void unbound_key(unsigned code) override {
    if (!alerts) return;
    std::string lbl = key_label(code);
    show_alert(lbl.empty() ? "That key isn't bound" : lbl + " isn't bound", 0.9);
  }

 private:
  static CGPoint cursor() {
    CGEventRef e = CGEventCreate(nullptr);
    CGPoint p = CGEventGetLocation(e);
    CFRelease(e);
    return p;
  }
  static CGPoint clamp(CGPoint p) {
    CGRect b = bounds();
    if (p.x < CGRectGetMinX(b)) p.x = CGRectGetMinX(b);
    if (p.y < CGRectGetMinY(b)) p.y = CGRectGetMinY(b);
    if (p.x > CGRectGetMaxX(b) - 1) p.x = CGRectGetMaxX(b) - 1;
    if (p.y > CGRectGetMaxY(b) - 1) p.y = CGRectGetMaxY(b) - 1;
    return p;
  }
  static CGRect bounds() {
    uint32_t n = 0;
    CGGetActiveDisplayList(0, nullptr, &n);
    if (n == 0) return CGRectMake(0, 0, 1440, 900);
    CGDirectDisplayID ids[16];
    if (n > 16) n = 16;
    CGGetActiveDisplayList(n, ids, &n);
    CGRect r = CGDisplayBounds(ids[0]);
    for (uint32_t i = 1; i < n; i++) r = CGRectUnion(r, CGDisplayBounds(ids[i]));
    return r;
  }
};

// ---- global backend state (the tap callback needs it) ----
struct State {
  Engine* engine = nullptr;
  MacOutput* out = nullptr;
  CFRunLoopTimerRef timer = nullptr;
  double interval = 0.016;
  ConfigSource cfg;
  std::string layer_src;  // hidutil source code currently installed
};
State* g = nullptr;
CFMachPortRef g_tap = nullptr;         // active event tap, or null while blocked
CFRunLoopSourceRef g_tap_src = nullptr;

void update_ui();  // defined below; tap_cb calls it before it appears
std::string exec_path();  // defined below

// True when this binary lives inside a .app bundle. Determined from the real
// executable path (symlinks resolved), not CFBundleGetMainBundle(): that keys
// off the *invoked* path, so launched through the `mousekeysd` symlink the cask
// installs it reports "no bundle" and we'd wrongly take the bare-binary path and
// self-register a stray LaunchAgent. LaunchServices runs an .app as a real app,
// so TCC lists it automatically and login is handled by SMAppService — no
// LaunchAgent handoff.
bool in_app_bundle() {
  return exec_path().find(".app/Contents/MacOS/") != std::string::npos;
}

// The config files to load right now. Re-scanned from the directory on each
// reload so drop-ins added or removed under config.d/ take effect live.
std::vector<std::string> current_files() {
  return g->cfg.dir.empty() ? g->cfg.files : config_files(g->cfg.dir);
}

double now_sec() {
  return static_cast<double>(clock_gettime_nsec_np(CLOCK_MONOTONIC)) / 1e9;
}

void compute_scale(double* sx, double* sy) {
  uint32_t n = 0;
  CGGetActiveDisplayList(0, nullptr, &n);
  if (n == 0) { *sx = *sy = 1; return; }
  CGDirectDisplayID ids[16];
  if (n > 16) n = 16;
  CGGetActiveDisplayList(n, ids, &n);
  CGRect prim = CGDisplayBounds(CGMainDisplayID());
  double refW = std::max(prim.size.width, prim.size.height);
  double refH = std::min(prim.size.width, prim.size.height);
  CGRect all = CGDisplayBounds(ids[0]);
  for (uint32_t i = 1; i < n; i++) all = CGRectUnion(all, CGDisplayBounds(ids[i]));
  *sx = all.size.width / refW;
  *sy = all.size.height / refH;
}

void apply_scale() {
  if (!g || !g->engine) return;
  const Settings& st = g->engine->settings();
  double sx, sy;
  if (st.scale_x > 0 && st.scale_y > 0) {
    sx = st.scale_x;
    sy = st.scale_y;
  } else {
    compute_scale(&sx, &sy);
  }
  g->engine->set_scale(sx, sy);
}

void tick_cb(CFRunLoopTimerRef, void*) {
  if (!g || !g->engine) return;
  static double last = 0;
  double t = now_sec();
  double dt = last > 0 ? t - last : g->interval;
  last = t;
  g->engine->tick(t, dt);
  if (!g->engine->wants_ticks()) {
    if (g->timer) {
      CFRunLoopTimerInvalidate(g->timer);
      CFRelease(g->timer);
      g->timer = nullptr;
    }
    last = 0;
  }
}

void ensure_timer() {
  if (g->timer) return;
  CFRunLoopTimerContext ctx = {0, nullptr, nullptr, nullptr, nullptr};
  g->timer = CFRunLoopTimerCreate(kCFAllocatorDefault, 0, g->interval, 0, 0,
                                  tick_cb, &ctx);
  CFRunLoopAddTimer(CFRunLoopGetCurrent(), g->timer, kCFRunLoopCommonModes);
}

CGEventRef tap_cb(CGEventTapProxy, CGEventType type, CGEventRef event, void*) {
  if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
    // The system disables a slow tap; re-enable so we keep receiving keys.
    if (g_tap) CGEventTapEnable(g_tap, true);
    return event;
  }
  if (!g || !g->engine) return event;

  bool down = (type == kCGEventKeyDown);
  bool up = (type == kCGEventKeyUp);
  bool flags = (type == kCGEventFlagsChanged);
  if (!down && !up && !flags) return event;

  unsigned kc = static_cast<unsigned>(
      CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
  unsigned mods = flags_to_mods(CGEventGetFlags(event));
  bool is_mod = is_modifier_keycode(kc);

  // flagsChanged fires for modifier press AND release with no up/down bit;
  // derive it from whether the modifier's flag is now set.
  bool key_down;
  if (flags) {
    CGEventFlags f = CGEventGetFlags(event);
    switch (kc) {
      case 56: case 60: key_down = f & kCGEventFlagMaskShift; break;
      case 59: case 62: key_down = f & kCGEventFlagMaskControl; break;
      case 58: case 61: key_down = f & kCGEventFlagMaskAlternate; break;
      case 55: case 54: key_down = f & kCGEventFlagMaskCommand; break;
      default: key_down = false; break;
    }
  } else {
    key_down = down;
  }

  Engine::Verdict v = g->engine->on_key(kc, key_down, mods, is_mod, now_sec());

  if (g->engine->panic()) {
    fprintf(stderr, "mousekeysd: panic chord — exiting\n");
    menubar_stop();
    return event;
  }

  update_ui();
  if (v == Engine::Verdict::Consume) {
    if (g->engine->wants_ticks()) ensure_timer();
    return nullptr;  // swallow
  }
  return event;
}

// ---- hidutil remap ----
void run_hidutil(const std::string& json) {
  std::string cmd = "hidutil property --set '" + json + "' >/dev/null 2>&1";
  int rc = system(cmd.c_str());
  (void)rc;
}
void install_remap(const std::string& src) {
  if (src.empty()) return;
  run_hidutil("{\"UserKeyMapping\":[{\"HIDKeyboardModifierMappingSrc\":" + src +
              ",\"HIDKeyboardModifierMappingDst\":" + kF18Dst + "}]}");
}
void clear_remap() { run_hidutil("{\"UserKeyMapping\":[]}"); }

// ---- config load / reload ----
bool build_engine(std::string* err) {
  Settings st;
  if (!load_settings_files(current_files(), &st, err)) return false;
  auto keymap = build_keymap();
  auto resolve = [&keymap](const std::string& n) -> unsigned {
    auto it = keymap.find(n);
    return it == keymap.end() ? kNoKey : it->second;
  };
  Bindings b;
  if (!make_bindings(st, resolve, &b, err)) return false;
  b.panic[0] = 56;   // left shift
  b.panic[1] = 60;   // right shift
  b.panic[2] = 53;   // escape
  g->out->alerts = st.alerts;
  set_alert_position(st.alert_position);
  g->interval = st.interval;

  // The Caps Lock remap is tied to the tap being live (see start_tap/
  // stop_tap), so Caps Lock is never hijacked while we lack permission. Here
  // we just record the target, and re-apply on the fly if the layer key
  // changed while the tap is already running.
  std::string new_src = hidutil_src(st.layer_key);
  if (g_tap && new_src != g->layer_src) {
    if (!g->layer_src.empty()) clear_remap();
    install_remap(new_src);
  }
  g->layer_src = new_src;

  if (g->engine) {
    g->engine->reconfigure(st, b);
  } else {
    g->engine = new Engine(st, b, *g->out);
  }
  apply_scale();
  return true;
}

// Push current engine state to the menu bar icon, but only on a change so
// the per-keystroke call is cheap.
void update_ui() {
  if (!g || !g->engine) return;
  static UiState last = UiState::Off;
  UiState s = g->engine->layer_toggled()
                  ? UiState::Locked
                  : (g->engine->layer_active() ? UiState::Active : UiState::Off);
  if (s != last) {
    last = s;
    menubar_set_state(s);
  }
}

void reload() {
  std::string err;
  if (build_engine(&err)) {
    show_alert("Config reloaded", 1.2);
  } else {
    fprintf(stderr, "mousekeysd: reload failed: %s\n", err.c_str());
    show_alert("Config error, kept the previous:\n" + err, 3.0);
  }
}

void menu_quit() { menubar_stop(); }

// Open the primary config file in the default text editor.
void menu_edit_config() {
  std::string path;
  if (!g->cfg.dir.empty())
    path = g->cfg.dir + "/config";
  else if (!g->cfg.files.empty())
    path = g->cfg.files.front();
  if (path.empty()) return;
  int rc = system(("open -t '" + path + "' >/dev/null 2>&1").c_str());
  (void)rc;
}

// "Start at login" is a LaunchAgent this daemon writes for itself (the
// bare-binary equivalent of SMAppService, which needs a .app). Presence of
// the plist controls whether launchd starts it at the next login; we don't
// bootstrap/bootout here, so toggling never starts a duplicate or kills the
// running session.
std::string login_plist_path() {
  const char* home = getenv("HOME");
  if (!home || !*home) return "";
  return std::string(home) +
         "/Library/LaunchAgents/space.existin.mousekeys.plist";
}

std::string exec_path() {
  char buf[4096];
  uint32_t sz = sizeof buf;
  if (_NSGetExecutablePath(buf, &sz) != 0) return "";
  char resolved[4096];
  return realpath(buf, resolved) ? std::string(resolved) : std::string(buf);
}

bool login_enabled() {
  if (in_app_bundle()) return app_login_enabled();  // SMAppService
  std::string p = login_plist_path();
  return !p.empty() && access(p.c_str(), F_OK) == 0;
}

void set_login(bool on) {
  if (in_app_bundle()) {  // .app: use SMAppService, not a LaunchAgent
    app_set_login(on);
    return;
  }
  std::string p = login_plist_path();
  if (p.empty()) return;
  if (on) {
    std::string exe = exec_path();
    if (exe.empty()) return;
    std::string dir = p.substr(0, p.rfind('/'));
    if (system(("mkdir -p '" + dir + "'").c_str()) != 0) return;
    FILE* f = fopen(p.c_str(), "w");
    if (!f) return;
    // MOUSEKEYS_MANAGED marks the launchd-run instance, so a manual run can
    // tell it should hand off rather than run in the wrong context.
    fprintf(f,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\"><dict>\n"
            "  <key>Label</key><string>space.existin.mousekeys</string>\n"
            "  <key>ProgramArguments</key><array><string>%s</string></array>\n"
            "  <key>EnvironmentVariables</key><dict>"
            "<key>MOUSEKEYS_MANAGED</key><string>1</string></dict>\n"
            "  <key>RunAtLoad</key><true/>\n"
            // Restart only on a crash, never on a clean quit — so the menu's
            // Quit (exit 0) actually stops it instead of being respawned.
            "  <key>KeepAlive</key><dict>"
            "<key>SuccessfulExit</key><false/></dict>\n"
            "  <key>ProcessType</key><string>Interactive</string>\n"
            "</dict></plist>\n",
            exe.c_str());
    fclose(f);
  } else {
    remove(p.c_str());
  }
}

// Boot out and delete a stray LaunchAgent, if one exists. Inside the .app this
// only ever cleans up an agent left by a bare run (an older build, or a run
// through the mousekeysd symlink before in_app_bundle() was path-based): the
// .app itself uses SMAppService and never writes one. The label targeted here
// is the LaunchAgent's own — the SMAppService instance runs under a different
// launchd label (application.space.existin.mousekeys.*), so it is untouched.
void remove_login_agent() {
  std::string p = login_plist_path();
  if (p.empty() || access(p.c_str(), F_OK) != 0) return;  // nothing to clean
  std::string uid = std::to_string(getuid());
  int rc = system(
      ("launchctl bootout gui/" + uid + "/space.existin.mousekeys 2>/dev/null")
          .c_str());
  (void)rc;
  remove(p.c_str());
}

// A manual `mousekeysd` run (not launched by our LaunchAgent) is treated as
// setup: register the login item and start the managed instance under
// launchd, then exit. This is the bare-binary equivalent of a .app login
// item — launchd runs the real instance in the correct GUI/Accessibility
// context, so the user never has to run it in a terminal.
int register_and_launch() {
  set_login(true);
  std::string uid = std::to_string(getuid());
  std::string plist = login_plist_path();
  std::string boot = "launchctl bootstrap gui/" + uid + " '" + plist + "' 2>/dev/null";
  if (system(boot.c_str()) != 0) {
    // Already loaded: restart it so it picks up this (possibly new) binary.
    std::string kick = "launchctl kickstart -k gui/" + uid + "/space.existin.mousekeys 2>/dev/null";
    int rc = system(kick.c_str());
    (void)rc;
  }
  fprintf(stderr,
          "mousekeysd: registered as a login agent and started.\n"
          "Grant Accessibility when prompted; it runs now and at every login.\n"
          "Manage it from the menu-bar icon (Start at Login, Quit).\n");
  return 0;
}

void display_reconfig_cb(CGDirectDisplayID, CGDisplayChangeSummaryFlags, void*) {
  apply_scale();
}

// Signal handlers can't touch CoreFoundation directly, so they set flags a
// run-loop timer polls. SIGHUP reloads; SIGINT/SIGTERM stop the loop so the
// single cleanup path (ungrab-equivalent: clear the hidutil remap) always
// runs — otherwise a killed daemon would leave Caps Lock remapped to F18
// until reboot.
volatile sig_atomic_t g_hup = 0;
volatile sig_atomic_t g_quit = 0;
void on_hup(int) { g_hup = 1; }
void on_quit(int) { g_quit = 1; }
void hup_check_cb(CFRunLoopTimerRef, void*) {
  if (g_hup) {
    g_hup = 0;
    reload();
  }
  if (g_quit) {
    menubar_stop();
  }
}

// Watch the config location (a directory, or a single -c file) and reload
// on any change — edits, atomic-rename saves, or added/removed drop-ins.
void install_config_watch() {
  if (g->cfg.watch.empty()) return;
  CFStringRef path = CFStringCreateWithCString(nullptr, g->cfg.watch.c_str(),
                                               kCFStringEncodingUTF8);
  CFArrayRef paths = CFArrayCreate(nullptr, (const void**)&path, 1, &kCFTypeArrayCallBacks);
  FSEventStreamContext ctx = {0, nullptr, nullptr, nullptr, nullptr};
  auto cb = [](ConstFSEventStreamRef, void*, size_t, void*,
               const FSEventStreamEventFlags*, const FSEventStreamEventId*) {
    reload();
  };
  FSEventStreamRef stream = FSEventStreamCreate(
      nullptr, cb, &ctx, paths, kFSEventStreamEventIdSinceNow, 0.2,
      kFSEventStreamCreateFlagFileEvents);
  // The main queue drains on the main thread under CFRunLoopRun, so the
  // reload callback shares that thread with the tap callback — no race on
  // the engine, and no deprecated run-loop scheduling.
  FSEventStreamSetDispatchQueue(stream, dispatch_get_main_queue());
  FSEventStreamStart(stream);
  CFRelease(paths);
  CFRelease(path);
}

// ---- Accessibility (TCC) gate ----
// macOS has no allow/deny-and-done dialog for Accessibility: an event tap
// that alters/suppresses keys requires the user to enable this process in
// System Settings ▸ Privacy & Security ▸ Accessibility, authenticated.
// AXIsProcessTrustedWithOptions(prompt) shows only a redirect dialog
// ("Open System Settings" / "Deny"); the toggle itself is manual. So the
// daemon prompts, deep-links the pane, and polls until the grant lands —
// then starts the tap with no restart. A signed, stable binary identity is
// what makes that grant survive upgrades (see the Homebrew formula).

bool ax_trusted(bool prompt) {
  const void* keys[] = {kAXTrustedCheckOptionPrompt};
  const void* vals[] = {prompt ? kCFBooleanTrue : kCFBooleanFalse};
  CFDictionaryRef opts = CFDictionaryCreate(
      nullptr, keys, vals, 1, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  bool trusted = AXIsProcessTrustedWithOptions(opts);
  CFRelease(opts);
  return trusted;
}

bool start_tap() {
  if (g_tap) return true;
  CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) |
                     CGEventMaskBit(kCGEventKeyUp) |
                     CGEventMaskBit(kCGEventFlagsChanged);
  // Tail-append (not head-insert): sit at the END of the session tap chain so
  // keystroke visualizers like KeyCastr, which head-insert, see mouse-mode
  // keys before we consume them. We still consume before the focused app.
  g_tap = CGEventTapCreate(kCGSessionEventTap, kCGTailAppendEventTap,
                           kCGEventTapOptionDefault, mask, tap_cb, nullptr);
  if (!g_tap) return false;
  g_tap_src = CFMachPortCreateRunLoopSource(nullptr, g_tap, 0);
  CFRunLoopAddSource(CFRunLoopGetMain(), g_tap_src, kCFRunLoopCommonModes);
  CGEventTapEnable(g_tap, true);
  install_remap(g->layer_src);  // hijack Caps Lock only while functional
  return true;
}

void stop_tap() {
  if (!g_tap) return;
  CGEventTapEnable(g_tap, false);
  if (g_tap_src) {
    CFRunLoopRemoveSource(CFRunLoopGetMain(), g_tap_src, kCFRunLoopCommonModes);
    CFRelease(g_tap_src);
    g_tap_src = nullptr;
  }
  CFMachPortInvalidate(g_tap);
  CFRelease(g_tap);
  g_tap = nullptr;
  clear_remap();                 // restore Caps Lock
  if (g->engine) g->engine->shutdown();  // drop any held keys/buttons
}

// Runs for the whole session and reconciles the tap with the live
// Accessibility grant in BOTH directions: starts the tap when permission is
// granted (including right after the first-run prompt), and tears it down —
// restoring Caps Lock and flipping the icon to "blocked" — if the user
// revokes permission while running. Re-granting brings it back with no
// restart.
void trust_monitor_cb(CFRunLoopTimerRef, void*) {
  bool trusted = ax_trusted(false);
  if (trusted && !g_tap) {
    if (start_tap()) {
      menubar_set_state(UiState::Off);
      show_alert("Ready", 1.0);
    }
  } else if (!trusted && g_tap) {
    stop_tap();
    menubar_set_state(UiState::Blocked);
    show_alert("Accessibility turned off", 2.5);
  }
}

}  // namespace

// Used by `--deregister-login` (the cask uninstall runs it before removing the
// app): drop every login mechanism so nothing lingers. Unregisters the .app's
// SMAppService item and also boots out + deletes any legacy LaunchAgent a bare
// or symlinked run may have left behind.
void deregister_login() {
  if (in_app_bundle()) app_set_login(false);
  app_reset_autoenable_guard();  // so a reinstall auto-enables again
  remove_login_agent();
}

int run_backend(const ConfigSource& cfg, bool foreground) {
  // Bare binary only: a plain run sets up the login agent and hands off to
  // launchd. Inside a .app, run directly (LaunchServices already gave us the
  // right context).
  if (!in_app_bundle() && !foreground && getenv("MOUSEKEYS_MANAGED") == nullptr)
    return register_and_launch();

  g = new State();
  g->out = new MacOutput();
  g->cfg = cfg;

  // First launch of the .app enables Start at Login once, so a fresh install
  // runs at every login without a manual toggle (the bare binary does the
  // equivalent in register_and_launch). A later manual disable is respected.
  // Also clear any stray LaunchAgent a pre-fix bare/symlinked run may have left,
  // so it can't launch a duplicate at login alongside the SMAppService instance.
  if (in_app_bundle()) {
    remove_login_agent();
    app_autoenable_login_once();
  }

  // Must exist before the event tap and run loop: it creates the
  // NSApplication whose event loop we run below.
  menubar_init({&reload, &menu_quit, &menu_edit_config, &login_enabled, &set_login});

  std::string err;
  if (!build_engine(&err)) {
    fprintf(stderr, "mousekeysd: %s\n", err.c_str());
    return 1;
  }

  // Housekeeping that needs no tap runs regardless of the permission state.
  CGDisplayRegisterReconfigurationCallback(display_reconfig_cb, nullptr);
  install_config_watch();
  signal(SIGHUP, on_hup);
  signal(SIGINT, on_quit);
  signal(SIGTERM, on_quit);
  // Safety net: restore Caps Lock on any graceful exit, including a Quit Apple
  // event (what `brew uninstall` sends) that terminates NSApp via exit() and so
  // skips the post-run-loop cleanup below.
  atexit(clear_remap);
  CFRunLoopTimerContext tctx = {0, nullptr, nullptr, nullptr, nullptr};
  CFRunLoopTimerRef hup_timer = CFRunLoopTimerCreate(
      kCFAllocatorDefault, 0, 0.25, 0, 0, hup_check_cb, &tctx);
  CFRunLoopAddTimer(CFRunLoopGetCurrent(), hup_timer, kCFRunLoopCommonModes);

  if (ax_trusted(false)) {
    if (start_tap()) show_alert("Ready", 1.0);
  } else {
    // Not yet trusted: show the menu-bar "blocked" state and fire the system
    // Accessibility prompt exactly once — its own "Open System Settings"
    // button handles navigation. No extra HUD, no auto-opened pane.
    menubar_set_state(UiState::Blocked);
    ax_trusted(true);
  }

  // Persistent monitor: picks up the grant after the first-run prompt, and
  // reacts to later revoke / re-grant without a restart.
  CFRunLoopTimerContext mctx = {0, nullptr, nullptr, nullptr, nullptr};
  CFRunLoopTimerRef mon = CFRunLoopTimerCreate(
      kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + 1.5, 1.5, 0, 0,
      trust_monitor_cb, &mctx);
  CFRunLoopAddTimer(CFRunLoopGetMain(), mon, kCFRunLoopCommonModes);

  menubar_run();  // AppKit event loop; returns on menubar_stop()

  stop_tap();      // disables tap, restores Caps Lock, drops held state
  clear_remap();   // idempotent: also covers the never-tapped case
  return 0;
}

}  // namespace mk
