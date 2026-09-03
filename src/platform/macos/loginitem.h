#pragma once

// Login-at-startup for the .app build, via ServiceManagement (SMAppService).
// Registering the running app as a login item is what Hammerspoon does; unlike
// a LaunchAgent it appears in System Settings > General > Login Items. Only
// meaningful when running inside the .app bundle. Implemented in loginitem.mm.
namespace mk {
bool app_login_enabled();   // true if registered as a login item
bool app_set_login(bool on);  // register / unregister; returns success
}  // namespace mk
