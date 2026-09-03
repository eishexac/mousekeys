# Contributing to mousekeys

Thanks for your interest. mousekeys is a small, dependency-free C++17 codebase;
contributions that keep it that way are the easiest to accept.

## Development

```sh
make            # build build/mousekeysd for the host OS
make check      # build and run the core unit tests (any platform)
make build      # macOS: the full daemon
```

Run the daemon in the foreground while hacking so it doesn't register a login
agent or hand off to launchd:

```sh
./build/mousekeysd -f -c ./mousekeys.conf
```

## Layout

- `src/core/` — the portable engine: config parsing, the movement/scroll/click
  math, and the QMK-style layer state machine. **Platform-free and
  unit-tested** (`test/test_core.cpp`); no OS headers here.
- `src/platform/macos/` — CGEventTap input, CGEvent output, the menu-bar item,
  the alert HUD, and login-agent self-registration.
- `src/platform/linux/` — evdev/uinput backend (in progress).
- `docs/` — background notes (keyboard ghosting, references).
- `legacy/` — the original Hammerspoon spoon.

## Ground rules

- **No third-party dependencies.** The core uses only the C++17 standard
  library; platform backends use only OS frameworks. This keeps the daemon
  auditable — it reads your keystrokes, so that matters.
- **Keep the core portable.** Logic that isn't OS-specific goes in `src/core/`
  behind the `Output`/resolver seams, with a unit test. Bugs are cheaper to
  catch there than in a backend.
- **Match the surrounding style.** Two-space indent, existing naming, comments
  that state constraints rather than narrate the code.
- `make check` must pass. Add or update tests for behavior changes.

## Pull requests

Branch from `dev`, keep changes focused, and describe the behavior change and
how you tested it. CI runs `make check` (all platforms) and `make build`
(macOS) on every PR.

## Reporting bugs and security issues

Functional bugs: open an issue. Security issues (this is a keyboard-reading
daemon): see [SECURITY.md](SECURITY.md) — please report privately.
