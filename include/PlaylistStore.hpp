#pragma once
#include <filesystem>
#include <string>
#include <vector>

class PlaylistStore {
public:
  PlaylistStore();

  std::vector<std::string> list() const;
  bool create(const std::string &name);
  bool remove(const std::string &name);
  bool rename(const std::string &oldName, const std::string &newName);

  // Returns false when the playlist cannot be read; a readable empty playlist
  // is a success with an empty track list.
  bool load(const std::string &name,
            std::vector<std::filesystem::path> &tracks) const;
  bool save(const std::string &name,
            const std::vector<std::filesystem::path> &tracks);
  bool addTrack(const std::string &name, const std::filesystem::path &track);

private:
  std::filesystem::path fileFor(const std::string &name) const;
  std::filesystem::path dir_;
};
