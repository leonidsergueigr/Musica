#include "PlaylistStore.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace fs = std::filesystem;

PlaylistStore::PlaylistStore() {
  const char *xdg = std::getenv("XDG_CONFIG_HOME");
  const char *home = std::getenv("HOME");
  fs::path base = xdg ? fs::path(xdg) : fs::path(home ? home : ".") / ".config";
  dir_ = base / "musica" / "playlists";
  std::error_code ec;
  fs::create_directories(dir_, ec);
}

fs::path PlaylistStore::fileFor(const std::string &n) const {
  return dir_ / (n + ".m3u");
}

std::vector<std::string> PlaylistStore::list() const {
  std::vector<std::string> out;
  std::error_code ec;
  for (fs::directory_iterator it(dir_, ec), end; !ec && it != end;
       it.increment(ec))
    if (it->path().extension() == ".m3u")
      out.push_back(it->path().stem().string());
  std::sort(out.begin(), out.end());
  return out;
}

bool PlaylistStore::create(const std::string &n) {
  if (n.empty() || n.find('/') != std::string::npos || fs::exists(fileFor(n)))
    return false;
  return save(n, {});
}

bool PlaylistStore::remove(const std::string &n) {
  std::error_code ec;
  return fs::remove(fileFor(n), ec);
}

bool PlaylistStore::rename(const std::string &a, const std::string &b) {
  std::error_code ec;
  if (fs::exists(fileFor(b)))
    return false;
  fs::rename(fileFor(a), fileFor(b), ec);
  return !ec;
}

std::vector<fs::path> PlaylistStore::load(const std::string &n) const {
  std::vector<fs::path> out;
  std::ifstream f(fileFor(n));
  std::string line;
  while (std::getline(f, line))
    if (!line.empty() && line[0] != '#')
      out.emplace_back(line);
  return out;
}

bool PlaylistStore::save(const std::string &n, const std::vector<fs::path> &t) {
  std::ofstream f(fileFor(n), std::ios::trunc);
  if (!f)
    return false;
  f << "#EXTM3U\n";
  for (auto &p : t)
    f << p.string() << '\n';
  return true;
}

bool PlaylistStore::addTrack(const std::string &n, const fs::path &track) {
  auto tracks = load(n);
  tracks.push_back(fs::absolute(track));
  return save(n, tracks);
}
