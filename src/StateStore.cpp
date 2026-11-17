#include "StateStore.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

fs::path configHome() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
    return fs::path(xdg);
  if (const char *home = std::getenv("HOME"); home && *home)
    return fs::path(home) / ".config";
  return fs::current_path() / ".config";
}

std::string trim(std::string s) {
  const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
  s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
  return s;
}

bool toBool(const std::string &s) { return s == "1" || s == "true"; }

} // namespace

fs::path statePath() { return configHome() / "musica" / "state"; }

SavedState loadState() {
  SavedState state;
  std::ifstream in(statePath());
  if (!in)
    return state;

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    const auto eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    const std::string key = trim(line.substr(0, eq));
    const std::string value = trim(line.substr(eq + 1));

    if (key == "last_dir") {
      state.lastDir = value;
    } else if (key == "volume") {
      try {
        state.volume = std::clamp(std::stof(value), 0.0f, 1.0f);
      } catch (...) {
        // Keep the default on a malformed value.
      }
    } else if (key == "shuffle") {
      state.shuffle = toBool(value);
    } else if (key == "repeat") {
      if (value == "all" || value == "1")
        state.repeat = 1;
      else if (value == "one" || value == "2")
        state.repeat = 2;
      else
        state.repeat = 0;
    } else if (key == "lang") {
      if (value == "fr" || value == "en")
        state.lang = value;
    }
  }
  return state;
}

bool saveState(const SavedState &state) {
  std::error_code ec;
  const fs::path path = statePath();
  fs::create_directories(path.parent_path(), ec);
  if (ec)
    return false;

  std::ofstream out(path, std::ios::trunc);
  if (!out)
    return false;

  const char *repeat = state.repeat == 1 ? "all"
                       : state.repeat == 2 ? "one"
                                           : "off";
  out << "# Musica state\n"
      << "last_dir=" << state.lastDir << "\n"
      << "volume=" << state.volume << "\n"
      << "shuffle=" << (state.shuffle ? 1 : 0) << "\n"
      << "repeat=" << repeat << "\n"
      << "lang=" << state.lang << "\n";
  return static_cast<bool>(out);
}
