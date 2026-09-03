#import <Cocoa/Cocoa.h>

#include "platform/macos/alert.h"

// A single reusable borderless HUD panel: the mousekeys mark on the left, a
// message on the right. Rebuilt on demand and reused; each call resets the
// fade so rapid alerts (e.g. several unbound keys) coalesce into one panel
// instead of stacking. Vertical placement is configurable (top/center/bottom).

namespace {
// Vertical: 0 top, 1 center, 2 bottom. Horizontal: 0 left, 1 center, 2 right.
// Default center-center.
int g_pos_v = 1;
int g_pos_h = 1;

// Fallback mark (keycap with a knocked-out cursor) as a TEMPLATE image, so the
// HUD tints it to the theme's label color — used only when there is no app
// bundle to load the shipped AppIcon from (e.g. a bare binary).
NSImage *fallback_mark(CGFloat s) {
  CGFloat k = s / 18.0;
  NSImage *img =
      [NSImage imageWithSize:NSMakeSize(s, s)
                     flipped:YES
              drawingHandler:^BOOL(NSRect r) {
                (void)r;
                NSBezierPath *cap = [NSBezierPath
                    bezierPathWithRoundedRect:NSMakeRect(1.5 * k, 1.5 * k, 15 * k, 15 * k)
                                      xRadius:4.5 * k
                                      yRadius:4.5 * k];
                const CGFloat pts[7][2] = {{0, 0},   {0, 12},   {3.2, 9.2}, {5.3, 14},
                                           {7.3, 13.1}, {5.2, 8.3}, {9.2, 8.3}};
                NSBezierPath *cur = [NSBezierPath bezierPath];
                CGFloat ox = 4.6 * k, oy = 3.0 * k, cs = 0.72 * k;
                [cur moveToPoint:NSMakePoint(ox + pts[0][0] * cs, oy + pts[0][1] * cs)];
                for (int i = 1; i < 7; i++)
                  [cur lineToPoint:NSMakePoint(ox + pts[i][0] * cs, oy + pts[i][1] * cs)];
                [cur closePath];
                [[NSColor blackColor] set];
                [cap fill];
                [NSGraphicsContext currentContext].compositingOperation =
                    NSCompositingOperationDestinationOut;
                [cur fill];  // knock the cursor out of the keycap
                [NSGraphicsContext currentContext].compositingOperation =
                    NSCompositingOperationSourceOver;
                return YES;
              }];
  [img setTemplate:YES];
  return img;
}

// The alert's icon: the shipped AppIcon.icns when running as an app bundle (the
// mark we ship — chrome keycap, Caps Lock legend, pointer), else the template
// fallback above.
NSImage *alert_icon(CGFloat s) {
  NSString *p = [[NSBundle mainBundle] pathForResource:@"AppIcon" ofType:@"icns"];
  if (p) {
    NSImage *img = [[NSImage alloc] initWithContentsOfFile:p];
    if (img) {
      img.size = NSMakeSize(s, s);
      return img;
    }
  }
  return fallback_mark(s);
}
}  // namespace

@interface MKAlertPanel : NSObject
@property(nonatomic, strong) NSPanel *panel;
@property(nonatomic, strong) NSTextField *label;
@property(nonatomic, strong) NSImageView *icon;
- (void)showText:(NSString *)text seconds:(double)seconds;
@end

@implementation MKAlertPanel

- (void)build {
  NSTextField *label = [[NSTextField alloc] initWithFrame:NSZeroRect];
  label.editable = NO;
  label.selectable = NO;
  label.bordered = NO;
  label.drawsBackground = NO;
  label.alignment = NSTextAlignmentLeft;
  label.usesSingleLineMode = NO;
  label.maximumNumberOfLines = 0;
  // Rounded system font (matches the keycap aesthetic) at semibold; dynamic
  // label color so it reads on both light and dark HUD material.
  NSFont *font = [NSFont systemFontOfSize:16 weight:NSFontWeightSemibold];
  if (@available(macOS 11.0, *)) {
    NSFontDescriptor *d =
        [font.fontDescriptor fontDescriptorWithDesign:NSFontDescriptorSystemDesignRounded];
    if (d) font = [NSFont fontWithDescriptor:d size:16];
  }
  label.font = font;
  label.textColor = [NSColor labelColor];
  self.label = label;

  NSImageView *icon = [[NSImageView alloc] initWithFrame:NSZeroRect];
  icon.image = alert_icon(30);
  // Tint only the template fallback; the real AppIcon keeps its own colors.
  if ([icon.image isTemplate]) icon.contentTintColor = [NSColor labelColor];
  self.icon = icon;

  NSPanel *panel = [[NSPanel alloc]
      initWithContentRect:NSMakeRect(0, 0, 320, 76)
                styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
                  backing:NSBackingStoreBuffered
                    defer:YES];
  panel.floatingPanel = YES;
  panel.level = NSStatusWindowLevel;
  panel.opaque = NO;
  panel.backgroundColor = [NSColor clearColor];
  panel.hasShadow = YES;
  panel.ignoresMouseEvents = YES;
  panel.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                             NSWindowCollectionBehaviorFullScreenAuxiliary |
                             NSWindowCollectionBehaviorStationary;

  NSVisualEffectView *bg = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
  // Popover material adapts to the system appearance (light / dark / auto),
  // unlike the always-dark HUD material.
  bg.material = NSVisualEffectMaterialPopover;
  bg.state = NSVisualEffectStateActive;
  bg.wantsLayer = YES;
  bg.layer.cornerRadius = 18;
  bg.layer.masksToBounds = YES;
  panel.contentView = bg;
  [bg addSubview:icon];
  [bg addSubview:label];

  self.panel = panel;
}

- (void)showText:(NSString *)text seconds:(double)seconds {
  if (!self.panel) [self build];

  const CGFloat pad = 20, iconSize = 30, gap = 14;
  self.label.stringValue = text;
  [self.label sizeToFit];
  NSSize ls = self.label.frame.size;

  CGFloat contentH = MAX(iconSize, ls.height);
  CGFloat h = contentH + 34;
  CGFloat w = MAX(200, pad + iconSize + gap + ls.width + pad);

  // visibleFrame excludes the menu bar and Dock, so corners/edges sit inside
  // usable space.
  NSScreen *screen = [NSScreen mainScreen];
  NSRect vf = screen ? screen.visibleFrame : NSMakeRect(0, 0, 1440, 900);
  const CGFloat margin = 28;
  CGFloat x = g_pos_h == 0 ? vf.origin.x + margin
              : g_pos_h == 2 ? vf.origin.x + vf.size.width - w - margin
                             : vf.origin.x + (vf.size.width - w) / 2;
  CGFloat y = g_pos_v == 0 ? vf.origin.y + vf.size.height - h - margin
              : g_pos_v == 2 ? vf.origin.y + margin
                             : vf.origin.y + (vf.size.height - h) / 2;
  [self.panel setFrame:NSMakeRect(x, y, w, h) display:YES];
  self.icon.frame = NSMakeRect(pad, (h - iconSize) / 2, iconSize, iconSize);
  self.label.frame = NSMakeRect(pad + iconSize + gap, (h - ls.height) / 2, ls.width, ls.height);

  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  self.panel.alphaValue = 1.0;
  [self.panel orderFrontRegardless];
  [self performSelector:@selector(fade) withObject:nil afterDelay:seconds];
}

- (void)fade {
  [NSAnimationContext runAnimationGroup:^(NSAnimationContext *ctx) {
    ctx.duration = 0.25;
    self.panel.animator.alphaValue = 0.0;
  }
      completionHandler:^{
        [self.panel orderOut:nil];
      }];
}

@end

namespace mk {

// Accepts a vertical and/or horizontal token in any order, separated by
// '_', '-', or space: e.g. "center", "top", "bottom_right", "top-left",
// "left". Unrecognized tokens leave that axis centered.
void set_alert_position(const std::string &pos) {
  int v = 1, h = 1;
  size_t start = 0;
  while (start <= pos.size()) {
    size_t sep = pos.find_first_of("_- ", start);
    std::string tok =
        pos.substr(start, sep == std::string::npos ? std::string::npos : sep - start);
    if (tok == "top")
      v = 0;
    else if (tok == "bottom")
      v = 2;
    else if (tok == "left")
      h = 0;
    else if (tok == "right")
      h = 2;
    // "center"/"centre"/"middle"/"" leave the axis centered
    if (sep == std::string::npos) break;
    start = sep + 1;
  }
  g_pos_v = v;
  g_pos_h = h;
}

void show_alert(const std::string &text, double seconds) {
  static MKAlertPanel *panel = nil;
  NSString *s = [NSString stringWithUTF8String:text.c_str()];
  // AppKit is main-thread only; hop explicitly so this is safe from anywhere.
  dispatch_async(dispatch_get_main_queue(), ^{
    if (!panel) panel = [[MKAlertPanel alloc] init];
    [panel showText:s seconds:seconds];
  });
}

}  // namespace mk
