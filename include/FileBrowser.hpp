#pragma once
#include <filesystem>
#include <vector>

struct BrowserEntry {
  std::filesystem::path path;
  std::string name;
  bool isDir;
};

class FileBrowser {
private:
  std::filesystem::path cwd_;
  std::vector<BrowserEntry> entries_;

public:
  explicit FileBrowser(
      std::filesystem::path start = std::filesystem::current_path());

  const std::filesystem::path &cwd() const { return cwd_; }
  const std::vector<BrowserEntry> &entries() const { return entries_; }

  bool enter(size_t index);
  void up();
  void refresh();

  std::vector<std::filesystem::path>
  audioFilesIn(const std::filesystem::path &dir, bool recursive = false) const;
};
