#pragma once

namespace mk {
// The commented default configuration, used both to seed a new user config
// on first run and to answer `--print-default-config`. One source, so the
// two never drift.
const char* default_config_text();
}  // namespace mk
