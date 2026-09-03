#import <ServiceManagement/ServiceManagement.h>

#include "platform/macos/loginitem.h"

namespace mk {

bool app_login_enabled() {
  if (@available(macOS 13.0, *))
    return SMAppService.mainAppService.status == SMAppServiceStatusEnabled;
  return false;
}

bool app_set_login(bool on) {
  if (@available(macOS 13.0, *)) {
    NSError *err = nil;
    BOOL ok = on ? [SMAppService.mainAppService registerAndReturnError:&err]
                 : [SMAppService.mainAppService unregisterAndReturnError:&err];
    return ok;
  }
  return false;
}

void app_autoenable_login_once() {
  if (@available(macOS 13.0, *)) {
    NSUserDefaults *d = [NSUserDefaults standardUserDefaults];
    if ([d boolForKey:@"MKAutoLoginDone"]) return;  // only the very first launch
    NSError *err = nil;
    // Mark "done" only if the register succeeded. Otherwise leave the guard
    // unset so the next launch retries — a failed register must not silently
    // disable Start at Login forever. A later manual disable still sticks: it
    // leaves the guard set (this succeeded once), so we never fight the user.
    if ([SMAppService.mainAppService registerAndReturnError:&err])
      [d setBool:YES forKey:@"MKAutoLoginDone"];
  }
}

// Forget that we auto-enabled. Run on uninstall so a later reinstall enables
// Start at Login again like a fresh install — the prefs holding the guard
// otherwise outlive a non-`--zap` uninstall.
void app_reset_autoenable_guard() {
  [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"MKAutoLoginDone"];
}

}  // namespace mk
