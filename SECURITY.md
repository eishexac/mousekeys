# Security

mousekeys reads keyboard input and synthesizes mouse events. On macOS that
requires Accessibility permission and means the daemon can see your keystrokes
while running. That makes its trustworthiness a real concern, so the code is
kept small and dependency-free specifically to be auditable in one sitting.

## Reporting a vulnerability

Use GitHub's private vulnerability reporting on this repository
(**Security → Report a vulnerability**). Please don't open a public issue for
anything that could expose input or escalate privileges before there's a fix.

There is no bounty and one maintainer. You'll get an honest answer, which may be
"yes, and I don't know when I'll fix it."

## Scope

In scope:

- Any path that lets the daemon capture or forward keystrokes beyond moving the
  cursor (e.g. logging key content, sending it anywhere).
- Privilege or permission escalation beyond the Accessibility grant the user
  gave.
- The login item or `hidutil` remap being writable or hijackable by another
  user/process.

Out of scope:

- Keyboard ghosting dropping keys — a hardware limitation, documented in
  [`docs/keyboard-ghosting.md`](docs/keyboard-ghosting.md).
- The daemon seeing keystrokes at all: that is inherent to an event-tap tool and
  is what the Accessibility prompt authorizes.

## Verifying releases

Release tags (`vX.Y.Z`) are signed with this key, and only this key:

```
eishexac <hexac@existin.space>
B387 26F0 61C1 AE22 E287  5F90 57ED 9D12 966B 397C
```

Fetch it from more than one channel and compare the fingerprint — the channels
are independent on purpose:

```sh
# the maintainer's site
curl -sS https://existin.space/openpgp/eishexac.asc | gpg --import
# or via WKD
gpg --locate-keys hexac@existin.space
# or GitHub's copy of the account's key (independent infrastructure)
curl -sS https://github.com/eishexac.gpg | gpg --import

gpg --fingerprint hexac@existin.space   # must match across all channels
```

Then:

```sh
git verify-tag v0.1.0
```

The macOS download carries its own independent checks — the signed tag is the
provenance anchor; these are what let the OS and Homebrew trust the file:

- **Developer ID + notarization.** The `.app` inside each DMG is signed with the
  maintainer's Apple Developer ID (Team `KQ342N2Y27`) and notarized by Apple.
  Inspect: `codesign -dvvv /Applications/mousekeys.app` and
  `xcrun stapler validate /Applications/mousekeys.app`.
- **Cask checksum.** `Casks/mousekeys.rb` in the tap pins the SHA-256 of each DMG.
- **Build provenance.** Each DMG has a GitHub build-provenance attestation (SLSA):
  `gh attestation verify mousekeys_<version>_<arch>.dmg --repo eishexac/mousekeys`.

Build from source and read it if in doubt.

## Status

Not audited. Published so the reasoning can be checked.
