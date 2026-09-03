# References

Prior art and specifications that shaped mousekeys.

## Layers and keyboard-driven mouse control

- [QMK Layers](https://docs.qmk.fm/feature_layers) — the layer system that
  inspired the activation model (MO for momentary, TT for tap-toggle).
- [QMK Mouse Keys](https://docs.qmk.fm/features/mouse_keys) — keyboard firmware
  mouse emulation with accelerated, kinetic, inertia, and constant modes.
- [peppy/qmk_firmware](https://github.com/peppy/qmk_firmware/tree/ppy_kinetic) —
  Dean Herbert's Planck keymap with kinetic mouse keys on a dedicated layer.

## Pointer acceleration

- [libinput pointer acceleration](https://wayland.freedesktop.org/libinput/doc/latest/pointer-acceleration.html)
  — Linux's piecewise-linear adaptive acceleration profile.
- [Apple IOHIDFamily](https://github.com/apple-oss-distributions/IOHIDFamily) —
  macOS HID event processing, including pointer acceleration curves.
- [X11 XKB MouseKeysAccel](https://www.x.org/releases/current/doc/kbproto/xkbproto.html)
  — the original keyboard mouse acceleration specification.
- [Mouse Ballistics (Coding Horror)](https://blog.codinghorror.com/mouse-ballistics/)
  — overview of pointer acceleration across operating systems.

## HID, key remapping, and ghosting

- [hidutil key remapping](https://developer.apple.com/library/archive/technotes/tn2450/_index.html)
  — Apple's technical note on HID key remapping.
- [USB HID Usage Tables](https://usb.org/document-library/hid-usage-tables-16) —
  the USB-IF spec defining the keyboard report format, modifier bitmap, and
  keycode assignments.
- [Rollover, blocking and ghosting (Deskthority)](https://deskthority.net/wiki/Rollover,_blocking_and_ghosting)
  — keyboard matrix ghosting, phantom keys, and how diodes prevent them.
- [Key rollover (Wikipedia)](https://en.wikipedia.org/wiki/Key_rollover) —
  N-key rollover, ghosting, and jamming in keyboard matrices.
- [How to make a keyboard — the matrix](http://blog.komar.be/how-to-make-a-keyboard-the-matrix/)
  — practical walkthrough of matrix design, scanning, and ghosting.

See [keyboard-ghosting.md](keyboard-ghosting.md) for how ghosting affects
mousekeys specifically.
