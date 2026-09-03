// mkicon — render the mousekeys app icon (a keycap + pointer, no background)
// at every size into an .iconset directory, for `iconutil -c icns`.
//
//   clang++ -fobjc-arc -framework Cocoa tools/mkicon.mm -o build/mkicon
//   ./build/mkicon build/mousekeys.iconset          # final icon → iconset
//   ./build/mkicon out.png [opts...]                # single 512 preview
//   ./build/mkicon sheet.png sheet                  # concept contact sheet
//
// opts: rounded|sleek|classic (pointer shape), mac (white border + shadow +
//       tilt), glyph:X (draw modifier X as the keycap legend), mono, nooutline.
// The icon IS the keycap: a two-tone key filling the whole tile with a pointer
// centered on the face. Colors are the celestial omzppuccin flavor (peach
// keycap, crimson pointer); `mono` renders a two-tone gray keycap instead.
#import <Cocoa/Cocoa.h>

static NSColor *hexa(unsigned v, CGFloat a) {
  return [NSColor colorWithSRGBRed:((v >> 16) & 0xff) / 255.0
                            green:((v >> 8) & 0xff) / 255.0
                             blue:(v & 0xff) / 255.0
                            alpha:a];
}
static NSColor *hex(unsigned v) { return hexa(v, 1.0); }

enum { PTR_CLASSIC, PTR_ROUNDED, PTR_SLEEK, PTR_UP };
static const CGFloat kClassic[7][2] = {{0, 0},   {0, 32},   {8.5, 24.5}, {14, 37},
                                       {20, 34.4}, {14.5, 22}, {25, 22}};
static const CGFloat kSleek[7][2] = {{0, 0},   {0, 34},   {8, 26.5},  {12, 39.5},
                                     {17, 37.5}, {12.5, 24}, {21.5, 22}};
// A symmetric arrow pointing straight up (mirror-symmetric about x=0): equal
// wings, a straight flat bottom. Rotate it (gTilt) to lean it like a pointer.
static const CGFloat kUp[7][2] = {{0, 0},  {-12, 20}, {-5, 15}, {-4, 31},
                                  {4, 31}, {5, 15},   {12, 20}};

static unsigned kSkirt = 0xC9926F, kTop = 0xFAB795, kPointer = 0xF43E5C, kOutline = 0x8F1A2E;
static unsigned kMonoSkirt = 0x6F747B, kMonoTop = 0xA7ADB4, kMonoPointer = 0x23262B;
static unsigned kLegend = 0x9A6B4E;      // engraved keycap legend tone
static unsigned kGlyphDark = 0x7C5438;   // a dark shade of the peach keycap
static unsigned kGlyphDarkMono = 0x474B51;  // a dark shade of the gray keycap

static int gStyle = PTR_ROUNDED;
static bool gOutline = true;
static bool gMono = false;
static bool gMac = false;       // white border + drop shadow + tilt
static CGFloat gTilt = 0;       // degrees
static const char *gGlyph = 0;  // modifier legend, e.g. "⌘" (cmd)
static int gGlyphMode = 0;      // 0 legend-behind, 1 big-front + cursor badge
static bool gGlyphDarkShade = false;  // big glyph in a dark keycap shade vs crimson
static bool gPointerBlack = false;    // black pointer (macOS-style) instead of crimson
static bool gClick = false;           // radiating click-ripple arcs at the cursor tip
static CGFloat gBadgeFrac = 0.24;     // cursor-badge size as a fraction of the icon
static CGFloat gBadgeDX = 0.19;       // badge offset from face center (fractions of face)
static CGFloat gBadgeDY = 0.17;       // +y is down (y-down space); smaller/negative = up

static NSBezierPath *polyPath(NSPoint *P, int n, CGFloat r) {
  NSBezierPath *p = [NSBezierPath bezierPath];
  if (r <= 0.01) {
    [p moveToPoint:P[0]];
    for (int i = 1; i < n; i++) [p lineToPoint:P[i]];
    [p closePath];
    return p;
  }
  NSPoint s = NSMakePoint((P[n - 1].x + P[0].x) / 2, (P[n - 1].y + P[0].y) / 2);
  [p moveToPoint:s];
  for (int i = 0; i < n; i++)
    [p appendBezierPathWithArcFromPoint:P[i] toPoint:P[(i + 1) % n] radius:r];
  [p closePath];
  return p;
}

// Draw upright text centered at (cx,cy) inside the y-down icon space.
static void drawGlyphCentered(NSString *g, CGFloat cx, CGFloat cy, CGFloat pt, NSColor *color) {
  NSDictionary *attrs = @{
    NSFontAttributeName : [NSFont systemFontOfSize:pt weight:NSFontWeightSemibold],
    NSForegroundColorAttributeName : color
  };
  NSSize sz = [g sizeWithAttributes:attrs];
  [NSGraphicsContext saveGraphicsState];
  NSAffineTransform *t = [NSAffineTransform transform];  // undo the y-down flip locally
  [t translateXBy:0 yBy:2 * cy];
  [t scaleXBy:1 yBy:-1];
  [t concat];
  [g drawAtPoint:NSMakePoint(cx - sz.width / 2, cy - sz.height / 2) withAttributes:attrs];
  [NSGraphicsContext restoreGraphicsState];
}

static NSBezierPath *buildPointer(CGFloat cx, CGFloat cy, CGFloat targetH) {
  const CGFloat(*src)[2] = gStyle == PTR_SLEEK ? kSleek : gStyle == PTR_UP ? kUp : kClassic;
  CGFloat radius = (gStyle == PTR_ROUNDED || gStyle == PTR_UP) ? 1.7 : 0.0;
  CGFloat minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
  for (int i = 0; i < 7; i++) {
    minx = MIN(minx, src[i][0]); maxx = MAX(maxx, src[i][0]);
    miny = MIN(miny, src[i][1]); maxy = MAX(maxy, src[i][1]);
  }
  CGFloat cs = targetH / (maxy - miny);
  CGFloat ax = cx - (minx + maxx) / 2 * cs, ay = cy - (miny + maxy) / 2 * cs;
  NSPoint P[7];
  for (int i = 0; i < 7; i++) P[i] = NSMakePoint(ax + src[i][0] * cs, ay + src[i][1] * cs);
  NSBezierPath *p = polyPath(P, 7, radius * cs);
  if (gTilt != 0) {
    NSAffineTransform *r = [NSAffineTransform transform];
    [r translateXBy:cx yBy:cy];
    [r rotateByDegrees:gTilt];
    [r translateXBy:-cx yBy:-cy];
    [p transformUsingAffineTransform:r];
  }
  return p;
}

// The pointer's tip (its hotspot / click point) for a given placement.
static NSPoint pointerTip(CGFloat cx, CGFloat cy, CGFloat targetH) {
  const CGFloat(*src)[2] = gStyle == PTR_SLEEK ? kSleek : gStyle == PTR_UP ? kUp : kClassic;
  CGFloat minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
  for (int i = 0; i < 7; i++) {
    minx = MIN(minx, src[i][0]); maxx = MAX(maxx, src[i][0]);
    miny = MIN(miny, src[i][1]); maxy = MAX(maxy, src[i][1]);
  }
  CGFloat cs = targetH / (maxy - miny);
  return NSMakePoint(cx - (minx + maxx) / 2 * cs, cy - (miny + maxy) / 2 * cs);
}

// Click ripple: concentric arcs fanning off the tip (up-left, away from body).
static void drawClickArcs(NSPoint o, CGFloat S, NSColor *c) {
  [c set];
  for (int i = 0; i < 3; i++) {
    CGFloat r = S * 0.05 + i * S * 0.05;
    NSBezierPath *a = [NSBezierPath bezierPath];
    a.lineWidth = S * 0.022;
    a.lineCapStyle = NSLineCapStyleRound;
    [a appendBezierPathWithArcWithCenter:o radius:r startAngle:196 endAngle:258];
    [a stroke];
  }
}

static void drawPointerAt(CGFloat cx, CGFloat cy, CGFloat targetH, CGFloat S) {
  NSBezierPath *p = buildPointer(cx, cy, targetH);
  unsigned fill = gPointerBlack ? 0x151515 : (gMono ? kMonoPointer : kPointer);
  if (gMac) {
    [NSGraphicsContext saveGraphicsState];
    NSShadow *sh = [NSShadow new];
    sh.shadowBlurRadius = S * 0.022;
    sh.shadowOffset = NSMakeSize(0, -S * 0.006);
    sh.shadowColor = hexa(0x000000, 0.38);
    [sh set];
    [[NSColor whiteColor] set];
    p.lineWidth = targetH * 0.14;  // white border, ~macOS proportion
    p.lineJoinStyle = NSLineJoinStyleRound;
    [p stroke];
    [NSGraphicsContext restoreGraphicsState];
    [hex(fill) set];
    [p fill];
  } else {
    [hex(fill) set];
    [p fill];
    if (gOutline && !gMono) {
      [hex(kOutline) set];
      p.lineWidth = S * 0.0055;
      p.lineJoinStyle = NSLineJoinStyleRound;
      [p stroke];
    }
  }
}

// Draw the icon at pixel size S, in a y-down coordinate space (0,0 = top left).
static void drawIcon(CGFloat S) {
  unsigned skirt = gMono ? kMonoSkirt : kSkirt;
  unsigned top = gMono ? kMonoTop : kTop;

  CGFloat m = S * 0.035;
  NSRect skirtRect = NSMakeRect(m, m, S - 2 * m, S - 2 * m);
  [hex(skirt) set];
  [[NSBezierPath bezierPathWithRoundedRect:skirtRect
                                   xRadius:skirtRect.size.width * 0.2237
                                   yRadius:skirtRect.size.width * 0.2237] fill];

  CGFloat side = S * 0.10, topBev = S * 0.085, bot = S * 0.15;
  NSRect face = NSMakeRect(m + side, m + topBev, S - 2 * m - 2 * side,
                           S - 2 * m - topBev - bot);
  [hex(top) set];
  [[NSBezierPath bezierPathWithRoundedRect:face
                                   xRadius:face.size.width * 0.20
                                   yRadius:face.size.width * 0.20] fill];

  CGFloat fcx = NSMidX(face), fcy = NSMidY(face);
  if (gGlyph && gGlyphMode == 0)  // subtle engraved legend behind the pointer
    drawGlyphCentered(@(gGlyph), fcx, fcy, face.size.height * 0.60, hexa(kLegend, 0.5));

  if (gGlyph && gGlyphMode == 1) {  // big glyph, small cursor badge
    unsigned gc = gGlyphDarkShade ? (gMono ? kGlyphDarkMono : kGlyphDark)
                                  : (gMono ? kMonoPointer : kPointer);
    drawGlyphCentered(@(gGlyph), fcx, fcy, face.size.height * 0.66, hex(gc));
    CGFloat bx = fcx + face.size.width * gBadgeDX, by = fcy + face.size.height * gBadgeDY;
    if (gClick) drawClickArcs(pointerTip(bx, by, S * gBadgeFrac), S, hex(kPointer));
    drawPointerAt(bx, by, S * gBadgeFrac, S);
  } else {
    if (gClick) drawClickArcs(pointerTip(fcx, fcy, S * 0.40), S, hex(kPointer));
    drawPointerAt(fcx, fcy, S * 0.40, S);
  }
}

static void drawIconInRect(CGFloat x, CGFloat y, CGFloat size) {
  [NSGraphicsContext saveGraphicsState];
  NSAffineTransform *t = [NSAffineTransform transform];
  [t translateXBy:x yBy:y + size];
  [t scaleXBy:1 yBy:-1];
  [t concat];
  drawIcon(size);
  [NSGraphicsContext restoreGraphicsState];
}

static NSBitmapImageRep *newRep(int w, int h) {
  return [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:NULL pixelsWide:w pixelsHigh:h bitsPerSample:8
               samplesPerPixel:4 hasAlpha:YES isPlanar:NO
                colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
}

static void writePNG(int px, NSString *path) {
  NSBitmapImageRep *rep = newRep(px, px);
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:rep]];
  NSAffineTransform *flip = [NSAffineTransform transform];
  [flip translateXBy:0 yBy:px];
  [flip scaleXBy:1 yBy:-1];
  [flip concat];
  drawIcon(px);
  [NSGraphicsContext restoreGraphicsState];
  [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:path atomically:YES];
}

static void writeSheet(NSString *path) {
  struct Cell {
    const char *label; int style; bool mac; CGFloat tilt; const char *glyph; int gmode;
    bool mono; bool dark; bool black; bool click; CGFloat badge; CGFloat bdy;
  };
  Cell cells[4] = {
      {"current (t0)",  PTR_ROUNDED, true, 0,   "⇪", 1, true, true, true, false, 0.36, 0.17},
      {"up · tilt 8",   PTR_ROUNDED, true, -8,  "⇪", 1, true, true, true, false, 0.36, 0.06},
      {"up · tilt 14",  PTR_ROUNDED, true, -14, "⇪", 1, true, true, true, false, 0.36, 0.06},
      {"up · tilt 20",  PTR_ROUNDED, true, -20, "⇪", 1, true, true, true, false, 0.36, 0.06},
  };
  int cols = 4, rows = 1, icon = 190, margin = 40, gapX = 30, labelH = 40, rowGap = 26;
  int W = margin * 2 + cols * icon + (cols - 1) * gapX;
  int H = margin * 2 + rows * (icon + labelH) + (rows - 1) * rowGap;

  NSBitmapImageRep *rep = newRep(W, H);
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:rep]];
  [hex(0x16161C) set];
  NSRectFill(NSMakeRect(0, 0, W, H));
  NSDictionary *attrs = @{
    NSFontAttributeName : [NSFont systemFontOfSize:19 weight:NSFontWeightMedium],
    NSForegroundColorAttributeName : [NSColor colorWithWhite:0.86 alpha:1]
  };
  for (int i = 0; i < 4; i++) {
    int ci = i % cols, ri = i / cols;
    CGFloat cellTop = H - margin - ri * (icon + labelH + rowGap);
    CGFloat iy = cellTop - icon, ix = margin + ci * (icon + gapX);
    gStyle = cells[i].style; gMac = cells[i].mac; gTilt = cells[i].tilt;
    gGlyph = cells[i].glyph; gGlyphMode = cells[i].gmode; gMono = cells[i].mono;
    gGlyphDarkShade = cells[i].dark; gPointerBlack = cells[i].black;
    gClick = cells[i].click; gBadgeFrac = cells[i].badge; gBadgeDY = cells[i].bdy;
    gBadgeDX = 0.19; gOutline = true;
    drawIconInRect(ix, iy, icon);
    NSString *ls = @(cells[i].label);
    NSSize sz = [ls sizeWithAttributes:attrs];
    [ls drawAtPoint:NSMakePoint(ix + icon / 2 - sz.width / 2, iy - 28) withAttributes:attrs];
  }
  [NSGraphicsContext restoreGraphicsState];
  [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:path atomically:YES];
}

int main(int argc, char **argv) {
  @autoreleasepool {
    if (argc < 2) {
      fprintf(stderr, "usage: mkicon <out.iconset|out.png> [opts...]\n");
      return 2;
    }
    NSString *out = [NSString stringWithUTF8String:argv[1]];
    static char glyphbuf[16];
    for (int i = 2; i < argc; i++) {
      NSString *a = [NSString stringWithUTF8String:argv[i]];
      if ([a isEqualToString:@"rounded"]) gStyle = PTR_ROUNDED;
      else if ([a isEqualToString:@"sleek"]) gStyle = PTR_SLEEK;
      else if ([a isEqualToString:@"classic"]) gStyle = PTR_CLASSIC;
      else if ([a isEqualToString:@"up"]) gStyle = PTR_UP;
      else if ([a isEqualToString:@"nooutline"]) gOutline = false;
      else if ([a isEqualToString:@"mono"]) gMono = true;
      else if ([a isEqualToString:@"mac"]) gMac = true;
      else if ([a hasPrefix:@"tilt:"]) gTilt = [a substringFromIndex:5].doubleValue;
      else if ([a isEqualToString:@"darkglyph"]) gGlyphDarkShade = true;
      else if ([a isEqualToString:@"black"]) gPointerBlack = true;
      else if ([a isEqualToString:@"click"]) gClick = true;
      else if ([a hasPrefix:@"badge:"]) gBadgeFrac = [a substringFromIndex:6].doubleValue;
      else if ([a hasPrefix:@"bdy:"]) gBadgeDY = [a substringFromIndex:4].doubleValue;
      else if ([a hasPrefix:@"bdx:"]) gBadgeDX = [a substringFromIndex:4].doubleValue;
      else if ([a hasPrefix:@"glyph:"]) {
        strncpy(glyphbuf, [a substringFromIndex:6].UTF8String, 15);
        gGlyph = glyphbuf;
        gGlyphMode = 1;
      } else if ([a isEqualToString:@"sheet"]) {
        writeSheet(out); fprintf(stderr, "wrote sheet %s\n", argv[1]); return 0;
      }
    }

    if ([out.pathExtension caseInsensitiveCompare:@"png"] == NSOrderedSame) {
      writePNG(512, out);
      fprintf(stderr, "wrote %s\n", argv[1]);
      return 0;
    }

    [[NSFileManager defaultManager] createDirectoryAtPath:out
                              withIntermediateDirectories:YES attributes:nil error:nil];
    struct { const char *name; int px; } items[] = {
        {"icon_16x16.png", 16},      {"icon_16x16@2x.png", 32},
        {"icon_32x32.png", 32},      {"icon_32x32@2x.png", 64},
        {"icon_128x128.png", 128},   {"icon_128x128@2x.png", 256},
        {"icon_256x256.png", 256},   {"icon_256x256@2x.png", 512},
        {"icon_512x512.png", 512},   {"icon_512x512@2x.png", 1024},
    };
    for (auto &it : items)
      writePNG(it.px, [out stringByAppendingPathComponent:@(it.name)]);
    fprintf(stderr, "wrote %s\n", argv[1]);
    return 0;
  }
}
