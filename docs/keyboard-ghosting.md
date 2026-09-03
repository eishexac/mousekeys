# Keyboard ghosting

Keyboards without per-key diodes — most laptop built-ins (MacBook, ThinkPad
T14s) and membrane keyboards, and Apple Magic Keyboards — use a key matrix. When
three or more simultaneously held keys form a rectangle in that matrix, the
controller cannot tell which keys are actually pressed and silently drops one.
This is **ghosting**. The dropped key never reaches the OS, so no software —
mousekeys included — can detect or work around it.

For mousekeys this affects **momentary mode**: holding the layer key + a click
key + a direction key is three keys held at once. Which triples ghost depends on
the matrix layout, which varies by model and is usually undocumented. Common
Apple examples: Caps Lock + Q + W, and Caps Lock + Q + Up.

**Toggle mode is immune.** Tapping the layer key to latch mouse mode on means
only two physical keys are ever held (click + direction), and two keys cannot
form the rectangle that causes ghosting.

## How a keyboard matrix works

Keys are wired in a grid of rows and columns. The controller scans each column
and reads which rows are active:

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

*(Simplified — real matrix layouts are proprietary, but the principle holds.)*

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

## USB HID report format

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

The 8 modifier keys (Ctrl, Shift, Alt, Cmd × left/right) are physically wired on
dedicated scan lines and reported in a single bitmap byte — they bypass the key
matrix entirely. Caps Lock is not one of them.

## Why software remapping doesn't help

```
hidutil: Caps → F18

┌──────────┬──────────┬──────┬──────┬──────┐
│ Modifier │ Reserved │ Key1 │ Key2 │ Key3 │
│ 00000000 │          │  Q   │  W   │ F18  │  ← still 3 regular keycodes
│          │          │ 0x14 │ 0x1A │ 0x6D │
└──────────┘          └──────┴──────┴──────┘

Physical matrix is unchanged. Caps is still at (Row 1, Col 0).
hidutil only changes the keycode AFTER the matrix scan.
Ghost still happens → W gets dropped before it reaches the OS.
```

Remapping Caps Lock to a modifier key (e.g. Right Control) via `hidutil` also
does not help. The remapping happens after the HID report is constructed — by
then the ghosted key is already missing. Only keys physically wired as modifiers
bypass the matrix.

## Workarounds

1. **Use toggle mode** — tap the layer key to latch mouse mode on, so only two
   keys are held at a time.
2. **Change conflicting bindings** — move a click off a key that ghosts with
   your layer key + direction keys.
3. **Use a keyboard with N-key rollover (NKRO)** — per-key diodes prevent
   ghosting entirely.
