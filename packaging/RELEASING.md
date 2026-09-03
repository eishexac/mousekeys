# Releasing mousekeys (macOS)

Releases ship a Developer ID-signed `mousekeys.app` as a Homebrew Cask, so
`brew install --cask eishexac/tap/mousekeys` installs it without compiling. A
stable Developer ID identity keeps the macOS Accessibility grant valid across
upgrades; an ad-hoc or from-source build does not, because its identity is the
code hash, which changes on every build.

The cask lives in the `eishexac/homebrew-tap` repository (`Casks/mousekeys.rb`);
the release workflow rewrites it from `packaging/mousekeys.cask.rb` on each
release. Build locally with `make app`.

## Prerequisites

1. **Developer ID Application certificate** (paid Apple Developer Program). An
   Apple Development certificate cannot notarize or distribute. Create it once:
   - developer.apple.com/account → Certificates → **+** → **Developer ID
     Application** → upload a CSR (Keychain Access → Certificate Assistant →
     Request a Certificate…, "Saved to disk") → download and install.
   - Verify: `security find-identity -v -p codesigning` lists
     `Developer ID Application: <name> (TEAMID)`.
2. **Repository secrets** (Settings → Secrets → Actions), used by CI:
   `MACOS_CERT_P12` (base64 of the exported `.p12`), `MACOS_CERT_PASSWORD`,
   `KEYCHAIN_PASSWORD` (any string; names the temporary keychain), and
   `HOMEBREW_TAP_DEPLOY_KEY` (private half of a read-write deploy key on the tap).
3. **Notarization via App Store Connect API key** (optional, recommended — uses
   no Apple ID or email). In App Store Connect → Users and Access → Integrations
   → App Store Connect API, create a **Team key** with the **Developer** role and
   download its `.p8` (once only); note the **Key ID** and the **Issuer ID**.
   Set three secrets: `NOTARY_API_KEY` (`base64 -i AuthKey_XXXX.p8`),
   `NOTARY_API_KEY_ID`, `NOTARY_API_ISSUER_ID`. Homebrew Cask quarantines the
   installed app, so without notarization first launch is gated by Gatekeeper
   (right-click → Open once); with the key set, CI notarizes and staples and
   launch is clean.

## Per release

CI does the build on a pushed tag. Bump `VERSION` in the Makefile, then:

```
git tag -s v0.1.0 -m "mousekeys 0.1.0" && git push origin v0.1.0
```

The signed tag fires the `release` workflow (`.github/workflows/release.yml`),
which, for each arch (`arm64`, `x86_64`): builds the app (`make sign-app
MACOS_ARCH="-arch <arch>"`), Developer ID-signs it, packages a signed DMG,
notarizes and staples it when the notary secrets are present, and computes its
sha256. It then attests build provenance (SLSA) over both DMGs, uploads
`mousekeys_<version>_arm64.dmg` and `mousekeys_<version>_x86_64.dmg` to the
release, and rewrites `Casks/mousekeys.rb` in the tap with the new version and
both sha256s under `on_arm`/`on_intel` (dropping the old CLI formula).
`./release.sh` automates the bump/tag/push; `--retag` replaces an existing
tag.

Re-run for an existing tag without re-tagging: `gh workflow run release.yml -f
tag=v0.1.0`.

## Verification

- Build provenance of a published DMG:
  `gh attestation verify mousekeys_<version>_<arch>.dmg --repo eishexac/mousekeys`.
- Signature of the installed app:
  `codesign --verify --strict --verbose=2 /Applications/mousekeys.app` and
  `codesign -dvvv /Applications/mousekeys.app 2>&1 | grep Authority=`.
- Notarization staple: `xcrun stapler validate /Applications/mousekeys.app`.

## Notes

- Hardened Runtime (`--options runtime`) and a secure `--timestamp` are applied
  by `make sign-app`; both are required for notarization.
- A from-source `make app` build is unsigned/ad-hoc — it runs, but macOS
  re-prompts for Accessibility on each rebuild because the identity is the code
  hash. Re-sign locally to keep the grant across rebuilds:
  `make sign-app MOUSEKEYS_CODESIGN_ID="$ID"`, then reinstall to `~/Applications`.
- Mechanical test of the signing step without a Developer ID cert:
  `make sign-app MOUSEKEYS_CODESIGN_ID="Apple Development: <name> (TEAMID)"`.
