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
    [SMAppService.mainAppService registerAndReturnError:nil];
    [d setBool:YES forKey:@"MKAutoLoginDone"];
  }
}

}  // namespace mk
