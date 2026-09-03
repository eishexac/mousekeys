#pragma once
#include <string>
#include <vector>

namespace mk {

// Where the daemon reads configuration. Either a directory (scanned for
// `config` + `config.d/*.conf`, re-scanned on every reload so drop-ins can
// be added or removed live) or an explicit list of files (from `-c`). An
// empty ConfigSource means built-in defaults.
struct ConfigSource {
  std::string dir;                 // config directory; "" if not dir-based
  std::vector<std::string> files;  // explicit files, used when dir is empty
  std::string watch;               // path to watch for changes; "" = none
};

// Implemented once per platform; owns the event loop and does not return
// until shutdown. SIGHUP — and, on macOS, a file-watch on the config
// location — reloads in place. Returns the process exit code.
//
// foreground: run directly instead of the default self-register handoff. On
// macOS a plain run registers a login agent and hands off to launchd (the
// correct Accessibility context); --foreground runs in place for debugging.
// Platforms without a login-agent concept (Linux/dom0) ignore it.
int run_backend(const ConfigSource& cfg, bool foreground);

// Unregister the login item and exit — used by the cask's uninstall before the
// app is removed, so it does not linger in Login Items. (A normal quit must not
// do this, or it would disable Start-at-Login every time the app quits.)
void deregister_login();

}  // namespace mk
