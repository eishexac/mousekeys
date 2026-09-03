#include "core/config.h"

#include <dirent.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace mk {

static std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

// Section and key names are case-insensitive; canonicalize to lower case.
static std::string to_lower(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

// Strip a trailing comment: '#' at line start or preceded by whitespace.
// A '#' glued to text is left alone.
static std::string strip_comment(const std::string& s) {
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '#' && (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t'))
      return s.substr(0, i);
  }
  return s;
}

bool Config::parse(const std::string& text, std::string& err) {
  std::string section;
  size_t pos = 0;
  int lineno = 0;
  while (pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    std::string line =
        text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    pos = nl == std::string::npos ? text.size() + 1 : nl + 1;
    lineno++;

    line = trim(strip_comment(line));
    if (line.empty()) continue;

    if (line.front() == '[') {
      if (line.back() != ']' || line.size() < 3) {
        err = "line " + std::to_string(lineno) + ": malformed section header";
        return false;
      }
      section = to_lower(trim(line.substr(1, line.size() - 2)));
      continue;
    }

    size_t eq = line.find('=');
    if (eq == std::string::npos) {
      err = "line " + std::to_string(lineno) + ": expected 'key = value'";
      return false;
    }
    std::string key = to_lower(trim(line.substr(0, eq)));
    std::string val = trim(line.substr(eq + 1));
    if (key.empty()) {
      err = "line " + std::to_string(lineno) + ": empty key";
      return false;
    }
    values_[section.empty() ? key : section + "." + key] = val;
  }
  return true;
}

bool Config::parse_file(const std::string& path, std::string& err) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) {
    err = "cannot open " + path;
    return false;
  }
  std::string text;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
  fclose(f);
  return parse(text, err);
}

bool Config::has(const std::string& key) const { return values_.count(key) != 0; }

std::string Config::get(const std::string& key, const std::string& def) const {
  auto it = values_.find(key);
  return it == values_.end() ? def : it->second;
}

double Config::get_num(const std::string& key, double def) const {
  auto it = values_.find(key);
  if (it == values_.end()) return def;
  const char* s = it->second.c_str();
  char* end = nullptr;
  double v = strtod(s, &end);
  if (end == s || *end != '\0') {
    bad_.push_back(key);
    return def;
  }
  return v;
}

std::vector<std::string> Config::get_list(const std::string& key) const {
  std::vector<std::string> out;
  auto it = values_.find(key);
  if (it == values_.end()) return out;
  const std::string& v = it->second;
  size_t start = 0;
  for (;;) {
    size_t comma = v.find(',', start);
    std::string tok =
        v.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
    std::string t = trim(tok);
    if (!t.empty()) out.push_back(t);
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return out;
}

static bool ends_with(const std::string& s, const std::string& suf) {
  return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

std::vector<std::string> config_files(const std::string& dir) {
  std::vector<std::string> out;
  if (dir.empty()) return out;

  std::string base = dir + "/config";
  if (access(base.c_str(), R_OK) == 0) out.push_back(base);

  std::string dropdir = dir + "/config.d";
  DIR* d = opendir(dropdir.c_str());
  if (d) {
    std::vector<std::string> drops;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
      std::string name = e->d_name;
      if (name[0] != '.' && ends_with(name, ".conf"))
        drops.push_back(dropdir + "/" + name);
    }
    closedir(d);
    std::sort(drops.begin(), drops.end());  // LC_ALL=C byte order
    for (auto& f : drops) out.push_back(f);
  }
  return out;
}

}  // namespace mk
