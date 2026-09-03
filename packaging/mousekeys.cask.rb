# mousekeys — a Developer ID-signed .app shipped as a Cask, per-arch (arm64 and
# x86_64 DMGs). Built and kept up to date by the mousekeys release workflow,
# which fills in the version and both per-arch sha256s.
#
# The .app is the product: it carries the daemon at Contents/MacOS/mousekeys and,
# as a registered bundle, macOS lists it in Accessibility and Login Items on its
# own. It is Developer ID-signed (and notarized when notary secrets are set), so
# the Accessibility grant survives upgrades.
cask "mousekeys" do
  version "@VERSION@"

  on_arm do
    sha256 "@SHA_ARM64@"
    url "https://github.com/eishexac/mousekeys/releases/download/v#{version}/mousekeys_#{version}_arm64.dmg"
  end
  on_intel do
    sha256 "@SHA_X86_64@"
    url "https://github.com/eishexac/mousekeys/releases/download/v#{version}/mousekeys_#{version}_x86_64.dmg"
  end

  name "mousekeys"
  desc "Keyboard-driven mouse control (QMK-style layers)"
  homepage "https://github.com/eishexac/mousekeys"

  depends_on macos: :ventura  # minimum; SMAppService login item needs macOS 13+

  app "mousekeys.app"

  # Launch it right after install so it prompts for Accessibility immediately.
  # The app is notarized, so Gatekeeper doesn't block the auto-open.
  postflight do
    system_command "/usr/bin/open", args: ["#{appdir}/mousekeys.app"]
  end

  caveats <<~EOS
    mousekeys opens automatically after install and asks for Accessibility
    permission — a macOS requirement no installer can grant. Approve mousekeys at:
      System Settings > Privacy & Security > Accessibility

    Then tap Caps Lock to enter mouse mode. Manage it from the menu-bar icon
    (Start at Login, Edit Config, Reload Config, Quit).

    Config is created on first run and hot-reloads on save (no restart):
      ~/.config/mousekeys/config
    Drop-ins in ~/.config/mousekeys/config.d/*.conf are merged in sorted order.
  EOS
end
