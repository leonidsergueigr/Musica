#pragma once

#include <filesystem>

struct Track {
  std::filesystem::path path;
  std::string title;
  std::string artist;
  std::string album;
  int trackNumber = 0;
  int bitrate = 0;
  int durationSec = 0;
};

bool isAudioFile(const std::filesystem::path &p);
Track readTrack(const std::filesystem::path &p);
