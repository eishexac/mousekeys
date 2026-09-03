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

}  // namespace mk
