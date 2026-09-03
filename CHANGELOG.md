# Changelog

All notable changes to mousekeys are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.4] — 2026-09-03

### Fixed
- Start at Login could silently stay off after a fresh install: the first-launch
  auto-enable ignored a failed `SMAppService` register yet still marked itself
  done, so it never retried. It now records "done" only when the register
  succeeds, and retries on the next launch otherwise.
- A plain `brew uninstall` kept the "already auto-enabled once" guard in app
  prefs, so a later reinstall would not re-enable Start at Login. Uninstall now
  clears that guard (matching what `--zap` already did), so a reinstall behaves
  like a fresh install.

## [0.1.3] — 2026-09-03

### Fixed
- The `mousekeysd` symlink added in 0.1.2 could make the daemon think it was a
  bare binary (bundle detection keyed off the invoked path, not the real one),
  so running `mousekeysd` self-registered a stray LaunchAgent that ran a second
  instance at login and survived uninstall. Bundle detection now resolves the
  real executable path, so the symlink is handled as the `.app` it points into.
- `--deregister-login` now clears **both** login mechanisms (the SMAppService
  item and any legacy LaunchAgent), and the `.app` boots out and deletes a stray
  LaunchAgent on launch — so machines affected by the 0.1.2 issue self-heal.

## [0.1.2] — 2026-09-03

### Added
- The daemon is now on your `PATH` as `mousekeysd` (the cask symlinks the app's
  binary), so `mousekeysd --print-default-config` — the command the install
  caveats point at — works from the shell.

### Fixed
- `brew uninstall` now deregisters the Start-at-Login item before removing the
  app, instead of leaving an orphaned entry in Login Items. Only the app itself
  can unregister its `SMAppService` item, so the cask runs it with
  `--deregister-login` first.

## [0.1.1] — 2026-09-03

### Changed
- Config is now **overrides only**: first run seeds a short stub instead of the
  full default, so `~/.config/mousekeys/config` holds just your changes and the
  built-in defaults stay current across upgrades. See them all with
  `mousekeysd --print-default-config`. Your config is kept on uninstall — even
  `brew uninstall --zap` leaves it alone.

### Fixed
- Enable **Start at Login** on the app's first launch, so a fresh `brew install`
  runs at every login without a manual toggle (auto-enabled once; a later manual
  disable is respected).
- `brew uninstall` now quits the running menu-bar app, and Caps Lock is restored
  on any exit — an `atexit` safety net covers the Quit event brew sends.

## [0.1.0] — 2026-09-03

Initial release.

### Added
- Keyboard-driven mouse control with a QMK-inspired layer (Caps Lock by
  default): move, click, drag, and scroll from the home row.
- Native macOS `.app` menu-bar agent — lists itself in Accessibility, registers
  a Login Item via `SMAppService`, dims its icon when Accessibility is off, and
  shows themed HUD alerts (config reloads, unbound keys).
- Zero-dependency C++17 core (layers, sub-pixel movement, scroll, click/drag)
  with unit tests, behind an OS-agnostic backend seam.
- Homebrew cask install: Developer ID-signed, notarized, per-arch
  (arm64 + x86_64) DMGs, each with a GitHub SLSA build-provenance attestation.
- INI-style config with hot-reload and `~/.config/mousekeys/config.d/` drop-ins.

[0.1.3]: https://github.com/eishexac/mousekeys/releases/tag/v0.1.3
[0.1.2]: https://github.com/eishexac/mousekeys/releases/tag/v0.1.2
[0.1.1]: https://github.com/eishexac/mousekeys/releases/tag/v0.1.1
[0.1.0]: https://github.com/eishexac/mousekeys/releases/tag/v0.1.0
