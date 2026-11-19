#include "PlaylistStore.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
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

bool PlaylistStore::load(const std::string &n,
                         std::vector<fs::path> &out) const {
  out.clear();
  const fs::path file = fileFor(n);
  std::ifstream f(file);
  if (!f)
    return false;
  const fs::path base = file.parent_path();
  std::string line;
  while (std::getline(f, line)) {
    // M3U files written on Windows keep a CR before the LF.
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;
    const fs::path p(line);
    // Relative entries belong to the playlist's folder, not to the process
    // working directory.
    out.push_back(p.is_absolute() ? p : base / p);
  }
  if (f.bad()) {
    out.clear();
    return false;
  }
  return true;
}

bool PlaylistStore::save(const std::string &n, const std::vector<fs::path> &t) {
  const fs::path dest = fileFor(n);
  const fs::path dir = dest.parent_path();

  // Write next to the destination, then rename over it: the original is never
  // truncated, so a failed write leaves it untouched.
  static std::atomic<unsigned long> seq{0};
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path tmp;
  do {
    tmp = dir / (".musica-playlist-" + std::to_string(stamp) + "-" +
                 std::to_string(seq.fetch_add(1)) + ".tmp");
  } while (fs::exists(tmp));

  std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
  if (!f)
    return false;
  f << "#EXTM3U\n";
  for (auto &p : t)
    f << p.string() << '\n';
  f.close();
  std::error_code ec;
  if (f.fail()) {
    fs::remove(tmp, ec);
    return false;
  }

  fs::rename(tmp, dest, ec);
  if (ec) {
    fs::remove(tmp, ec);
    return false;
  }
  return true;
}

bool PlaylistStore::addTrack(const std::string &n, const fs::path &track) {
  std::vector<fs::path> tracks;
  if (!load(n, tracks))
    return false;
  tracks.push_back(fs::absolute(track));
  return save(n, tracks);
}
