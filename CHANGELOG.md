# Changelog

All notable changes to mousekeys are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.1] — 2026-09-03

### Fixed
- The app now enables **Start at Login** on its first launch, so a fresh
  `brew install` runs at every login without a manual toggle. A later manual
  disable is respected (it only auto-enables once).

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

[0.1.1]: https://github.com/eishexac/mousekeys/releases/tag/v0.1.1
[0.1.0]: https://github.com/eishexac/mousekeys/releases/tag/v0.1.0
