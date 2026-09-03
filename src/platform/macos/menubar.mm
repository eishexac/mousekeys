#import <Cocoa/Cocoa.h>

#include "platform/macos/menubar.h"

// Target for the menu actions; forwards to the C callbacks the backend
// registered. Retained for the process lifetime.
@interface MKMenuTarget : NSObject
@property(nonatomic, assign) mk::MenuHooks hooks;
@property(nonatomic, unsafe_unretained) NSMenuItem* loginItem;
@end

@implementation MKMenuTarget
- (void)reload:(id)sender {
  (void)sender;
  if (self.hooks.reload) self.hooks.reload();
}
- (void)quit:(id)sender {
  (void)sender;
  if (self.hooks.quit) self.hooks.quit();
}
- (void)editConfig:(id)sender {
  (void)sender;
  if (self.hooks.edit_config) self.hooks.edit_config();
}
- (void)toggleLogin:(id)sender {
  (void)sender;
  if (!self.hooks.set_login || !self.hooks.login_enabled) return;
  bool now = !self.hooks.login_enabled();
  self.hooks.set_login(now);
  self.loginItem.state = now ? NSControlStateValueOn : NSControlStateValueOff;
}
@end

namespace mk {
namespace {

NSStatusItem* g_item = nil;
MKMenuTarget* g_target = nil;
NSMenuItem* g_status_line = nil;

// The mousekeys mark: a keycap with a pointer cursor, drawn as a template
// image so the menu bar tints it for light/dark. State reads from fill
// weight — outline when off, solid (cursor knocked out) when active, plus a
// sticky dot when latched; the whole icon dims when blocked on permission.
NSBezierPath* cursor_path(CGFloat scale, NSPoint o) {
  // Classic pointer, tip at (0,0), in flipped (y-down) coordinates.
  const CGFloat pts[7][2] = {{0, 0}, {0, 12}, {3.2, 9.2}, {5.3, 14},
                             {7.3, 13.1}, {5.2, 8.3}, {9.2, 8.3}};
  NSBezierPath* p = [NSBezierPath bezierPath];
  [p moveToPoint:NSMakePoint(o.x + pts[0][0] * scale, o.y + pts[0][1] * scale)];
  for (int i = 1; i < 7; i++)
    [p lineToPoint:NSMakePoint(o.x + pts[i][0] * scale, o.y + pts[i][1] * scale)];
  [p closePath];
  return p;
}

NSImage* icon_for(UiState s) {
  const CGFloat side = 18;
  NSImage* img = [NSImage imageWithSize:NSMakeSize(side, side)
                                flipped:YES
                         drawingHandler:^BOOL(NSRect r) {
                           (void)r;
                           NSBezierPath* cap =
                               [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(1.5, 1.5, 15, 15)
                                                              xRadius:4.5
                                                              yRadius:4.5];
                           NSBezierPath* cur = cursor_path(0.72, NSMakePoint(4.6, 3.0));
                           [[NSColor blackColor] set];
                           bool solid = (s == UiState::Active || s == UiState::Locked);
                           if (solid) {
                             [cap fill];
                             [NSGraphicsContext currentContext].compositingOperation =
                                 NSCompositingOperationDestinationOut;
                             [cur fill];  // knock the cursor out of the keycap
                             [NSGraphicsContext currentContext].compositingOperation =
                                 NSCompositingOperationSourceOver;
                           } else {
                             cap.lineWidth = 1.6;
                             [cap stroke];
                             [cur fill];
                           }
                           if (s == UiState::Locked)
                             [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(12.2, 12.2, 3.8, 3.8)] fill];
                           // Blocked (Accessibility off) is shown by dimming the
                           // whole button in menubar_set_state, not by a slash.
                           return YES;
                         }];
  [img setTemplate:YES];
  return img;
}

}  // namespace

void menubar_init(const MenuHooks& hooks) {
  [NSApplication sharedApplication];
  // Accessory: a background agent with a menu-bar presence and no Dock icon.
  [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

  g_target = [[MKMenuTarget alloc] init];
  g_target.hooks = hooks;

  g_item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
  NSImage* img = icon_for(UiState::Off);
  if (img)
    g_item.button.image = img;
  else
    g_item.button.title = @"MK";  // fallback if drawing unavailable
  g_item.button.toolTip = @"mousekeys — mouse mode off";

  NSMenu* menu = [[NSMenu alloc] init];
  g_status_line = [[NSMenuItem alloc] initWithTitle:@"Mouse mode: off"
                                             action:nil
                                      keyEquivalent:@""];
  g_status_line.enabled = NO;
  [menu addItem:g_status_line];
  [menu addItem:[NSMenuItem separatorItem]];

  NSMenuItem* edit = [[NSMenuItem alloc] initWithTitle:@"Edit Config"
                                                action:@selector(editConfig:)
                                         keyEquivalent:@"e"];
  edit.target = g_target;
  [menu addItem:edit];

  NSMenuItem* reload = [[NSMenuItem alloc] initWithTitle:@"Reload Config"
                                                  action:@selector(reload:)
                                           keyEquivalent:@"r"];
  reload.target = g_target;
  [menu addItem:reload];

  NSMenuItem* login = [[NSMenuItem alloc] initWithTitle:@"Start at Login"
                                                 action:@selector(toggleLogin:)
                                          keyEquivalent:@""];
  login.target = g_target;
  login.state = (hooks.login_enabled && hooks.login_enabled())
                    ? NSControlStateValueOn
                    : NSControlStateValueOff;
  [menu addItem:login];
  g_target.loginItem = login;

  [menu addItem:[NSMenuItem separatorItem]];

  NSMenuItem* quit = [[NSMenuItem alloc] initWithTitle:@"Quit mousekeys"
                                                action:@selector(quit:)
                                         keyEquivalent:@"q"];
  quit.target = g_target;
  [menu addItem:quit];

  g_item.menu = menu;
}

void menubar_set_state(UiState s) {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (!g_item) return;
    NSImage* img = icon_for(s);
    if (img) g_item.button.image = img;
    // Gray the icon out (dim) while Accessibility is off, instead of a slash.
    g_item.button.alphaValue = (s == UiState::Blocked) ? 0.35 : 1.0;
    NSString* label;
    switch (s) {
      case UiState::Locked: label = @"Mouse mode: on (locked)"; break;
      case UiState::Active: label = @"Mouse mode: on"; break;
      case UiState::Blocked: label = @"Waiting for Accessibility…"; break;
      case UiState::Off: default: label = @"Mouse mode: off"; break;
    }
    g_status_line.title = label;
    g_item.button.toolTip = [@"mousekeys — " stringByAppendingString:label];
  });
}

// AppKit's own event loop, so status-item clicks and menu tracking work.
// The plain CFRunLoop would service our tap and timers but not dispatch the
// window-server events a menu needs.
void menubar_run() { [NSApp run]; }

void menubar_stop() {
  dispatch_async(dispatch_get_main_queue(), ^{
    [NSApp stop:nil];
    // -stop: only takes effect after the next event, so nudge one through.
    NSEvent* e = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                    location:NSZeroPoint
                               modifierFlags:0
                                   timestamp:0
                                windowNumber:0
                                     context:nil
                                     subtype:0
                                       data1:0
                                       data2:0];
    [NSApp postEvent:e atStart:YES];
  });
}

}  // namespace mk
