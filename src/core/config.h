#pragma once
#include <map>
#include <string>
#include <vector>

namespace mk {

// Flat key/value store parsed from INI text: "[section]" + "key = value"
// becomes "section.key". Comments start with '#' (';' is a bindable key, so
// it is not a comment character here). Unknown keys are ignored on lookup;
// a present-but-malformed number is recorded in bad_keys() so the caller
// can fail loudly instead of running with a silently-wrong default.
class Config {
public:
  bool parse(const std::string& text, std::string& err);
  bool parse_file(const std::string& path, std::string& err);

  bool has(const std::string& key) const;
  std::string get(const std::string& key, const std::string& def) const;
  double get_num(const std::string& key, double def) const;
  // Splits a value on commas, trimming each item and dropping empties. Used
  // for key bindings: "w, up" -> {"w", "up"} (primary, then secondary).
  std::vector<std::string> get_list(const std::string& key) const;

  const std::vector<std::string>& bad_keys() const { return bad_; }

private:
  std::map<std::string, std::string> values_;
  mutable std::vector<std::string> bad_;
};

// Config fragments to load from a directory, in load order: "<dir>/config"
// first (if present), then "<dir>/config.d/*.conf" sorted lexicographically
// (LC_ALL=C). Later files override earlier keys. Empty if none exist.
std::vector<std::string> config_files(const std::string& dir);

}  // namespace mk
