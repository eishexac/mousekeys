# mousekeys

Keyboard-driven mouse control for macOS via Hammerspoon. Hold or tap a layer key (Caps Lock by default) to enter mouse mode, then move the cursor, click, drag, and scroll — all from the keyboard. Uses a QMK-inspired layer system with tap-toggle support and zero modifier conflicts.

## Features

- Layer-based activation — dedicated key enters mouse mode, no modifier conflicts
- Tap-toggle — quick tap locks mouse mode on, tap again to exit (inspired by [QMK TT](https://docs.qmk.fm/feature_layers#switching-and-toggling-layers))
- Automatic key remapping via macOS built-in `hidutil` (Caps Lock → F18 by default, no third-party software)
- Time-based cursor movement — speed ramps with hold duration, instant stop on release, instant direction changes (no momentum)
- Sub-pixel accumulation for smooth movement at any speed
- Delta-time scaling for consistent behavior regardless of timer jitter
- Left click with hold-to-drag and double/triple click detection
- Right click
- Time-based pixel scrolling with glide on release
- Primary and optional secondary key bindings (e.g., WASD + arrow keys)
- Multi-display support — cursor moves freely across all connected screens with orientation-aware speed scaling

## Installation

Clone directly into your Spoons directory:

```
git clone https://github.com/hexac/mousekeys.git ~/.hammerspoon/Spoons/mousekeys.spoon
```

Or symlink from a local checkout:

```
ln -s ~/path/to/mousekeys ~/.hammerspoon/Spoons/mousekeys.spoon
```

## Setup

Create a setup file for the spoon and require it from your Hammerspoon `init.lua`:

```
~/.hammerspoon/
  init.lua
  setup/
    mousekeys.lua
```

**`~/.hammerspoon/setup/mousekeys.lua`**:

```lua
hs.loadSpoon("mousekeys")

-- Override any defaults (see Config options below)
-- spoon.mousekeys.remapKey = "capslock"
-- spoon.mousekeys.layerKey = "f18"
-- spoon.mousekeys.tapTimeout = 0.2

spoon.mousekeys:start()
```

**`~/.hammerspoon/init.lua`**:

```lua
require("setup.mousekeys")
```

## How it works

The spoon uses a layer-based system inspired by [QMK keyboard firmware layers](https://docs.qmk.fm/feature_layers). In QMK, holding a layer key temporarily activates an alternate keymap — the same physical keys produce different outputs. This spoon brings that concept to macOS: holding a dedicated layer key turns your keyboard into a mouse controller.

1. On start, `hidutil` remaps your physical key (`remapKey`) to a virtual key (`layerKey`) at the HID level — no LED toggle, no delay, no modifier behavior
2. **Hold** the layer key to enter mouse mode (momentary — active while held)
3. **Tap** the layer key to toggle mouse mode on permanently — tap again to toggle off
4. **Move** with your configured direction keys — taps are precise, sustained holds are fast
5. **Left click** — press the left click key (hold to drag)
6. **Right click** — press the right click key
7. **Scroll** — press scroll keys (tap for instant scroll, hold for continuous with glide)
8. **Release** the layer key to exit mouse mode (momentary only — toggle stays active)
9. On stop, `hidutil` restores the original key mapping

A press counts as a tap if it is released within `tapTimeout` (default 200ms) and no other layer keys were pressed during the hold. Any key use during a hold makes it strictly momentary.

### About hidutil

`hidutil` is a built-in macOS utility that remaps keys at the HID (hardware input device) level. This is the same level that Karabiner-Elements operates at, but `hidutil` requires no third-party software.

When the spoon starts, the physical key (`remapKey`) is remapped to a virtual key (`layerKey`) that exists only for the spoon to intercept. The physical key loses its original function entirely — for example, if you remap Caps Lock, it will no longer toggle caps. The virtual key (F18 by default) is a function key with no standard macOS behavior, chosen specifically because it won't conflict with anything.

The remapping:

- Happens below the OS keyboard processing layer — the original key behavior is completely replaced, not layered on top
- Produces clean keyDown/keyUp events that Hammerspoon can intercept
- Resets automatically on reboot (the original key is restored)
- Is restored when the spoon is stopped via `spoon.mousekeys:stop()`

If you want to keep the original key's behavior and handle remapping yourself (e.g., via Karabiner-Elements), set `remapKey` to `nil` to skip the hidutil step.

## Config options

### Layer key

| Option | Default | Description |
|---|---|---|
| `layerKey` | `"f18"` | Key that activates mouse mode (Hammerspoon key name) |
| `remapKey` | `"capslock"` | Physical key to remap to `layerKey` via hidutil |
| `tapTimeout` | `0.2` | Max hold duration (seconds) to count as a tap-toggle |

`remapKey` accepts friendly names or raw HID usage codes:

| Name | HID Code | Description |
|---|---|---|
| `"capslock"` | `0x700000039` | Caps Lock (default) |
| `"left_control"` | `0x7000000E0` | Left Control |
| `"left_shift"` | `0x7000000E1` | Left Shift |
| `"left_option"` | `0x7000000E2` | Left Option |
| `"left_command"` | `0x7000000E3` | Left Command |
| `"right_control"` | `0x7000000E4` | Right Control |
| `"right_shift"` | `0x7000000E5` | Right Shift |
| `"right_option"` | `0x7000000E6` | Right Option |
| `"right_command"` | `0x7000000E7` | Right Command |
| `"section"` | `0x700000064` | Section sign (§) |
| `"f18"` | `0x70000006D` | F18 (default layer key) |
| `"f19"` | `0x70000006E` | F19 |
| `"f20"` | `0x70000006F` | F20 |

Set `remapKey` to `nil` to skip hidutil remapping (if you handle it externally).

### Movement

Cursor movement uses a time-based model with no momentum. Speed is computed each frame as a function of how long the direction key has been held: `speed = initialSpeed + acceleration * holdTime`. Releasing a key stops movement on that axis instantly. Acceleration and max speed are scaled per-axis based on the total screen bounding box relative to the primary screen's landscape dimensions — this keeps cursor feel consistent across portrait monitors, multi-display setups, and mixed resolutions. Scale factors update automatically when displays are connected or disconnected.

| Option | Default | Description |
|---|---|---|
| `initialSpeed` | `100` | Cursor speed (px/sec) on first frame — controls tap precision |
| `acceleration` | `5000` | Speed ramp rate (px/sec²) — how fast it reaches max speed |
| `maxSpeed` | `4000` | Maximum cursor speed (px/sec) |
| `interval` | `0.016` | Timer interval in seconds (~60 fps) |

### Scrolling

Scrolling uses a time-based ramp while holding (like movement) with momentum glide on release. Speed is computed as `speed = scrollInitialSpeed + scrollAcceleration * holdTime`. On release, the last velocity decays through friction for a natural glide.

| Option | Default | Description |
|---|---|---|
| `scrollInitialSpeed` | `20` | Scroll speed (px/sec) on first frame — controls tap precision |
| `scrollAcceleration` | `800` | Scroll ramp rate (px/sec²) |
| `scrollMaxSpeed` | `600` | Maximum scroll speed (px/sec) |
| `scrollFriction` | `0.85` | Glide decay after release (0–1, higher = longer glide) |
| `scrollTap` | `1` | Instant scroll on first press (pixels * pixelScale) |
| `scrollPixelScale` | `4` | Pixel multiplier for scroll output |

### Controls

| Option | Default | Description |
|---|---|---|
| `keys` | `{ primary = { up="i", down="k", left="j", right="l" } }` | Movement keys (supports optional `secondary` table) |
| `leftClick` | `"u"` | Left mouse button key |
| `rightClick` | `"o"` | Right mouse button key |
| `scrollUp` | `{ primary = ";" }` | Scroll up key(s) |
| `scrollDown` | `{ primary = "/" }` | Scroll down key(s) |
| `scrollLeft` | `{ primary = "'" }` | Scroll left key(s) |
| `scrollRight` | `{ primary = "\\" }` | Scroll right key(s) |

## Apple keyboard ghosting

Apple keyboards (MacBook built-in and Magic Keyboards) use a key matrix without per-key diodes. When three or more simultaneously held keys form a rectangle in the matrix, the controller cannot distinguish which keys are actually pressed and silently drops one — this is called ghosting. The dropped key never reaches macOS, so no software (including this spoon) can detect or work around it.

This affects **momentary mode** (holding the layer key + click + direction = 3 keys). Common ghosting combinations on Apple keyboards include Caps Lock + Q + W and Caps Lock + Q + Up Arrow. The exact combinations depend on the matrix layout, which varies by model and is not publicly documented.

**Toggle mode is unaffected** — tapping the layer key to toggle mouse mode on means only two physical keys are held at a time (click + direction), which cannot ghost.

### How a keyboard matrix works

Keys are wired in a grid of rows and columns. The controller scans each column and reads which rows are active:

```
        Col 0      Col 1      Col 2      Col 3      Col 4
          │          │          │          │          │
Row 0 ────┼─[Tab]────┼─[Q]──────┼─[W]──────┼─[E]──────┼─[R]────
          │          │          │          │          │
Row 1 ────┼─[Caps]───┼─[A]──────┼─[S]──────┼─[D]──────┼─[F]────
          │          │          │          │          │
Row 2 ────┼─[Shift]──┼─[Z]──────┼─[X]──────┼─[C]──────┼─[V]────
          │          │          │          │          │
```

*(Simplified — Apple's actual matrix layout is proprietary, but the principle is the same)*

**Normal: 2 keys, no ghost**

```
Press [Q] + [S]: Different row AND different column — no ambiguity

        Col 0      Col 1      Col 2
          │          │          │
Row 0 ────┼──────────┼─[Q]◄─────┼──────────
          │          │          │
Row 1 ────┼──────────┼──────────┼─[S]◄─────
          │          │          │

No rectangle — reports Q, S ✓
```

**Ghosting: 3 keys that form a rectangle**

```
Press [Caps] + [Q] + [W]:

        Col 0      Col 1      Col 2
          │          │          │
Row 0 ────┼──────────┼─[Q]◄─────┼─[W]◄─────    ◄ Q and W on same row
          │          │          │
Row 1 ────┼─[Caps]◄──┼──────────┼──────────    ◄ Caps on same col as Tab
          │          │          │

Caps, Q, W form an L-shape. Current leaks through the matrix — the
controller sees a phantom keypress and drops the ambiguous key.
```

**No ghost: [Caps] + [Q] + [S]**

```
        Col 0      Col 1      Col 2
          │          │          │
Row 0 ────┼──────────┼─[Q]◄─────┼──────────
          │          │          │
Row 1 ────┼─[Caps]◄──┼──────────┼─[S]◄─────
          │          │          │

No rectangle — current paths are unambiguous. Reports Caps, Q, S ✓
```

### USB HID report format

The USB HID report separates modifier keys from regular keys:

```
┌────────────┬──────────┬─────────────────────────────────────────┐
│ Byte 0     │ Byte 1   │ Bytes 2-7                               │
│ MODIFIER   │ Reserved │ Keycode slots (up to 6 keys)            │
│ BITMAP     │          │                                         │
├────────────┤          ├──────┬──────┬──────┬──────┬──────┬──────┤
│ bit 0: L-Ctrl         │ Key1 │ Key2 │ Key3 │ Key4 │ Key5 │ Key6 │
│ bit 1: L-Shift        │      │      │      │      │      │      │
│ bit 2: L-Alt          │  Q   │  W   │ Caps │      │      │      │
│ bit 3: L-Cmd          │ 0x14 │ 0x1A │ 0x39 │ 0x00 │ 0x00 │ 0x00 │
│ bit 4: R-Ctrl         │      │      │  ▲   │      │      │      │
│ bit 5: R-Shift        │      │      │  │   │      │      │      │
│ bit 6: R-Alt          │      │      │  │   │      │      │      │
│ bit 7: R-Cmd          │      │      │  │   │      │      │      │
└────────────┘          └──────┴──────┴──┼───┴──────┴──────┴──────┘
                                         │
                        CapsLock is a REGULAR keycode —
                        it uses one of the 6 slots and
                        lives in the matrix like any letter key.
```

The 8 modifier keys (Ctrl, Shift, Alt, Cmd × left/right) are physically wired on dedicated scan lines and reported in a single bitmap byte — they bypass the key matrix entirely. Caps Lock is not one of them.

### Why software remapping doesn't help

```
hidutil: Caps → F18

┌──────────┬──────────┬──────┬──────┬──────┐
│ Modifier │ Reserved │ Key1 │ Key2 │ Key3 │
│ 00000000 │          │  Q   │  W   │ F18  │  ← still 3 regular keycodes
│          │          │ 0x14 │ 0x1A │ 0x6D │
└──────────┘          └──────┴──────┴──────┘

Physical matrix is unchanged. Caps is still at (Row 1, Col 0).
hidutil only changes the keycode AFTER the matrix scan.
Ghost still happens → W gets dropped before it reaches macOS.
```

Remapping Caps Lock to a modifier key (e.g., Right Control) via `hidutil` also does not work. The remapping happens after the HID report is constructed — by that point, the ghosted key is already missing. Only keys that are physically wired as modifiers bypass the matrix.

### Workarounds

1. **Use toggle mode** — tap the layer key to lock mouse mode on, then only two keys are held at a time
2. **Change conflicting bindings** — move left click off a key that ghosts with your layer key + direction keys
3. **Use an external keyboard** — mechanical keyboards with N-key rollover (NKRO) have per-key diodes and do not ghost

## Architecture

The spoon is split into focused modules:

- `init.lua` — configuration, event routing, layer key handling, hidutil remapping, shared timer
- `movement.lua` — cursor movement (time-based, no momentum)
- `scroll.lua` — scroll gestures (time-based ramp + glide on release)
- `click.lua` — click operations with multi-click detection

## References

- [QMK Layers](https://docs.qmk.fm/feature_layers) — the layer system that inspired this spoon's activation model (MO for momentary, TT for tap-toggle)
- [QMK Mouse Keys](https://docs.qmk.fm/features/mouse_keys) — keyboard firmware mouse emulation with accelerated, kinetic, inertia, and constant modes
- [peppy/qmk_firmware](https://github.com/peppy/qmk_firmware/tree/ppy_kinetic) — Dean Herbert's Planck keymap with kinetic mouse keys on a dedicated layer
- [hidutil key remapping](https://developer.apple.com/library/archive/technotes/tn2450/_index.html) — Apple's technical note on HID key remapping
- [libinput pointer acceleration](https://wayland.freedesktop.org/libinput/doc/latest/pointer-acceleration.html) — Linux's piecewise linear adaptive acceleration profile
- [Apple IOHIDFamily](https://github.com/apple-oss-distributions/IOHIDFamily) — macOS HID event processing including pointer acceleration curves
- [X11 XKB MouseKeysAccel](https://www.x.org/releases/current/doc/kbproto/xkbproto.html) — the original keyboard mouse acceleration specification
- [Mouse Ballistics (Coding Horror)](https://blog.codinghorror.com/mouse-ballistics/) — overview of pointer acceleration across operating systems
- [USB HID Usage Tables](https://usb.org/document-library/hid-usage-tables-16) — official USB-IF specification defining the keyboard report format, modifier bitmap, and keycode assignments
- [Rollover, blocking and ghosting (Deskthority)](https://deskthority.net/wiki/Rollover,_blocking_and_ghosting) — detailed explanation of keyboard matrix ghosting, phantom keys, and how diodes prevent them
- [Key rollover (Wikipedia)](https://en.wikipedia.org/wiki/Key_rollover) — overview of N-key rollover, ghosting, and jamming in keyboard matrices
- [How to make a keyboard — the matrix](http://blog.komar.be/how-to-make-a-keyboard-the-matrix/) — practical walkthrough of keyboard matrix design, scanning, and ghosting

## License

MIT
