#pragma once

// Menu bar status item (NSStatusItem): the persistent indicator of mouse
// mode. Unlike the transient alert, the icon always reflects current state,
// so the user can tell at a glance whether the layer is off, held, latched,
// or blocked on permission. Implemented in menubar.mm against AppKit;
// declared here for the C++ backend. Safe to call the setters from the main
// (run-loop) thread; they hop to the main queue internally if needed.
namespace mk {

enum class UiState { Off, Active, Locked, Blocked };

// Callbacks the menu invokes, all on the main thread.
struct MenuHooks {
  void (*reload)();          // Reload Config
  void (*quit)();            // Quit mousekeys
  void (*edit_config)();     // Edit Config (open in default editor)
  bool (*login_enabled)();   // current "start at login" state
  void (*set_login)(bool);   // enable/disable "start at login"
};

// Creates the NSApplication (accessory policy — no Dock icon) and the status
// item with its menu. Call once before the run loop.
void menubar_init(const MenuHooks& hooks);

// Updates the icon/tooltip to reflect engine state.
void menubar_set_state(UiState s);

// Runs AppKit's event loop (so menu tracking works) and returns when
// menubar_stop() is called. Replaces a bare CFRunLoopRun().
void menubar_run();
void menubar_stop();

}  // namespace mk
