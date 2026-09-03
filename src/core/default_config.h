#pragma once

namespace mk {
// The full commented default configuration, printed by `--print-default-config`
// so a user can copy any line they want to change. It is NOT seeded into the
// user's config: the same values are the built-in defaults (see Settings), so
// they stay current across upgrades instead of being frozen in a seeded file.
const char* default_config_text();

// The small "overrides only" stub seeded into ~/.config/mousekeys/config on
// first run: no active keys, just a header explaining that anything unset uses
// the built-in default and pointing at `--print-default-config`.
const char* config_stub_text();
}  // namespace mk
