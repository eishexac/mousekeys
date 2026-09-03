# mousekeys

Keyboard-driven mouse control. Hold or tap a layer key (Caps Lock by default)
to enter mouse mode, then move the cursor, click, drag, and scroll — all from
the home row. A native daemon with a QMK-inspired layer system, no third-party
runtime.

## Install (macOS)

```sh
brew install eishexac/tap/mousekeys
```

This installs **mousekeys.app** to `/Applications` (the tap ships only a cask, so
`brew` resolves it without `--cask`) and launches it — it runs as a menu-bar app.
On first launch macOS asks for **Accessibility** permission (required to read
keys and move the cursor) — approve **mousekeys** in **System Settings → Privacy
& Security → Accessibility**. It then starts, and because the app is notarized
and Developer ID-signed the grant persists across upgrades. Turn on **Start at
Login** from the menu-bar icon to run it at every login.

To build from source instead, clone the repo and run `make app` (see
[CONTRIBUTING](CONTRIBUTING.md)).

## Use

Tap **Caps Lock** to latch mouse mode on (tap again to exit), or hold it for
momentary mode. With the defaults:

| Keys | Action |
|---|---|
| `i` `j` `k` `l` | move up / left / down / right (tap = precise, hold = accelerate) |
| `u` | left click (hold to drag) |
| `o` | right click |
| `;` `/` `'` `\` | scroll up / down / left / right |
| both Shifts + `Esc` | force-quit mouse mode |

The menu-bar icon shows state at a glance: outline keycap when off, solid when
active, a dot when latched, a slash when Accessibility is off.

## Configure

Configuration lives in `~/.config/mousekeys/` (honors `$XDG_CONFIG_HOME`). A
commented default `config` is written on first run; edit and save and it
reloads automatically — no restart (or use the menu's **Edit Config**).
Regenerate the default any time:

```sh
mousekeysd --print-default-config
```

The format is sectioned INI. Key bindings are comma lists — `primary,
secondary` — and section/key names are case-insensitive:

```ini
[movement]
initial_speed = 100
acceleration  = 5000
up    = i
down  = k
left  = j
right = l

[click]
left  = u
right = o

[scroll]
tap   = 1
up    = ;
down  = /

[macos]
alert_position = center   # top | bottom | left | right | top_left | ...
```

Split settings across `~/.config/mousekeys/config.d/*.conf` if you like; files
merge in sorted order, later keys winning.

## How it works

A layer key (Caps Lock, remapped to a spare key via the built-in `hidutil`
while running) toggles an alternate keymap, the way [QMK keyboard
layers](https://docs.qmk.fm/feature_layers) do. Movement is time-based with no
momentum — speed ramps with how long a direction is held, stops instantly on
release, and accumulates sub-pixel remainders so slow speeds still move. Speed
scales per-axis to the screen layout. Scrolling adds a friction glide on
release. The Caps Lock remap is active only while the daemon holds Accessibility
permission, so the key is never hijacked when the tool can't function.

## Keyboard ghosting

On keyboards without per-key diodes (most laptop and membrane keyboards), three
held keys that form a rectangle in the scan matrix can drop one — the layer key
+ a click + a direction can ghost. Toggle mode avoids it. Full explanation with
matrix diagrams: [`docs/keyboard-ghosting.md`](docs/keyboard-ghosting.md).

## Building from source

```sh
make          # build build/mousekeysd
make check    # run the core unit tests (platform-independent)
```

C++17, no third-party dependencies. The engine (`src/core/`) is platform-free
and unit-tested; platform backends live in `src/platform/` (macOS via
CGEventTap; a Linux evdev/uinput backend is in progress). See
[CONTRIBUTING.md](CONTRIBUTING.md).

## Legacy

The original implementation was a [Hammerspoon](https://www.hammerspoon.org/)
spoon (Lua), preserved in [`legacy/`](legacy/). Its findings live on in
[`docs/`](docs/).

## License

MIT — see [LICENSE](LICENSE).
