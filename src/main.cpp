#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/config.h"
#include "core/default_config.h"
#include "platform/backend.h"

namespace {

bool readable(const std::string& p) { return access(p.c_str(), R_OK) == 0; }
bool is_dir(const std::string& p) {
  struct stat st;
  return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// $XDG_CONFIG_HOME, else ~/.config. "" if neither is available.
std::string config_home() {
  const char* x = getenv("XDG_CONFIG_HOME");
  if (x && *x) return x;
  const char* h = getenv("HOME");
  if (h && *h) return std::string(h) + "/.config";
  return "";
}

// mkdir -p, best effort.
void mkdirs(const std::string& dir) {
  std::string cur;
  for (size_t i = 0; i < dir.size(); i++) {
    cur += dir[i];
    if (dir[i] == '/' && cur.size() > 1) mkdir(cur.c_str(), 0755);
  }
  mkdir(dir.c_str(), 0755);
}

bool write_file(const std::string& path, const char* text) {
  FILE* f = fopen(path.c_str(), "wx");  // x: never clobber an existing file
  if (!f) return false;
  size_t n = fwrite(text, 1, strlen(text), f);
  bool ok = n == strlen(text);
  fclose(f);
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  std::string forced;  // explicit -c path (file or directory)
  bool forced_given = false;
  bool foreground = false;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-c" || a == "--config") {
      if (i + 1 >= argc) {
        fprintf(stderr, "mousekeysd: %s needs a path\n", a.c_str());
        return 2;
      }
      forced = argv[++i];
      forced_given = true;
    } else if (a == "-f" || a == "--foreground") {
      foreground = true;
    } else if (a == "--print-default-config") {
      fputs(mk::default_config_text(), stdout);
      return 0;
    } else if (a == "--deregister-login") {
      mk::deregister_login();  // internal: the cask uninstall calls this
      return 0;
    } else if (a == "--version") {
      printf("mousekeysd %s\n", MK_VERSION);
      return 0;
    } else if (a == "-h" || a == "--help") {
      printf(
          "usage: mousekeysd [-c <path>] [-f] [--print-default-config]\n"
          "\n"
          "Keyboard-driven mouse control. A plain run registers a login agent\n"
          "and starts it under launchd (the correct Accessibility context),\n"
          "then exits — so `mousekeysd` once is all the setup needed. Use -f /\n"
          "--foreground to run in place instead (for debugging).\n"
          "\n"
          "Configuration is read from a directory: ~/.config/mousekeys/\n"
          "(honors $XDG_CONFIG_HOME), or /etc/mousekeys/ when running as root.\n"
          "Within it, 'config' is read first, then every 'config.d/*.conf' in\n"
          "sorted order, with later keys overriding earlier ones. On first run\n"
          "a commented default 'config' is written for you. -c overrides with a\n"
          "specific file or directory. SIGHUP reloads; edits are also picked up\n"
          "automatically.\n"
          "\n"
          "While mouse mode is active, holding both Shift keys and pressing\n"
          "Escape exits immediately.\n");
      return 0;
    } else {
      fprintf(stderr, "mousekeysd: unknown argument '%s' (try --help)\n", a.c_str());
      return 2;
    }
  }

  mk::ConfigSource cfg;

  if (forced_given) {
    if (!readable(forced)) {
      fprintf(stderr, "mousekeysd: config path not found: %s\n", forced.c_str());
      return 1;
    }
    if (is_dir(forced)) {
      cfg.dir = forced;
      cfg.watch = forced;
    } else {
      cfg.files = {forced};
      cfg.watch = forced;
    }
    return mk::run_backend(cfg, foreground);
  }

  // Default: a per-user config directory. On first run seed only an
  // "overrides only" stub — the built-in defaults do the rest, so nothing is
  // duplicated and upgrades stay current. Root (the system/headless case, e.g.
  // dom0) reads /etc/mousekeys instead and is never seeded into a home dir.
  std::string home = config_home();
  std::string userdir = home.empty() ? "" : home + "/mousekeys";

  if (!userdir.empty() && !mk::config_files(userdir).empty()) {
    cfg.dir = userdir;
  } else if (!userdir.empty() && geteuid() != 0) {
    mkdirs(userdir);
    if (write_file(userdir + "/config", mk::config_stub_text()))
      fprintf(stderr, "mousekeysd: wrote config stub to %s/config\n",
              userdir.c_str());
    cfg.dir = userdir;  // even if the write failed, watch it for a later add
  } else if (is_dir("/etc/mousekeys")) {
    cfg.dir = "/etc/mousekeys";
  }
  // else: cfg stays empty -> built-in defaults.

  cfg.watch = cfg.dir;
  return mk::run_backend(cfg, foreground);
}
