#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

// Decoded RGB cover, downscaled to at most 64 px per side. Independent of the
// UI.
struct CoverArt {
  int w = 0;
  int h = 0;
  std::vector<uint8_t> rgb; // w * h * 3
};

// Uses the picture embedded in the tags first, then a cover/folder/front/album
// image (.jpg, .jpeg, .png) next to the track.
std::optional<CoverArt> loadCover(const std::filesystem::path &track);
