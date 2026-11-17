#include "FileBrowser.hpp"
#include "Track.hpp"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

FileBrowser::FileBrowser(fs::path start) : cwd_(fs::absolute(start)) {
  refresh();
}

void FileBrowser::refresh() {
  entries_.clear();
  std::error_code ec;
  for (fs::directory_iterator it(cwd_, ec), end; !ec && it != end;
       it.increment(ec)) {
    const auto &p = it->path();
    std::string name = p.filename().string();
    if (name.empty() || name[0] == '.')
      continue;

    bool dir = it->is_directory(ec);
    if (dir || isAudioFile(p))
      entries_.push_back({p, name, dir});
  }
  std::sort(entries_.begin(), entries_.end(), [](auto &a, auto &b) {
    if (a.isDir != b.isDir)
      return a.isDir;
    return std::lexicographical_compare(
        a.name.begin(), a.name.end(), b.name.begin(), b.name.end(),
        [](char x, char y) { return tolower(x) < tolower(y); });
  });
}

bool FileBrowser::enter(size_t index) {
  if (index >= entries_.size() || !entries_[index].isDir)
    return false;
  cwd_ = entries_[index].path;
  refresh();
  return true;
}

void FileBrowser::up() {
  if (cwd_.has_parent_path() && cwd_ != cwd_.root_path()) {
    cwd_ = cwd_.parent_path();
    refresh();
  }
}

std::vector<fs::path> FileBrowser::audioFilesIn(const fs::path &dir,
                                                bool recursive) const {
  std::vector<fs::path> out;
  std::error_code ec;
  auto collect = [&](auto it) {
    for (const auto &e : it)
      if (e.is_regular_file(ec) && isAudioFile(e.path()))
        out.push_back(e.path());
  };
  if (recursive)
    collect(fs::recursive_directory_iterator(dir, ec));
  else
    collect(fs::directory_iterator(dir, ec));
  std::sort(out.begin(), out.end());
  return out;
}
