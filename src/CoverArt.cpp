#include "CoverArt.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <taglib/fileref.h>
#include <taglib/toolkit/tbytevector.h>
#include <taglib/toolkit/tfile.h>
#include <taglib/toolkit/tvariant.h>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fs = std::filesystem;

namespace {

constexpr int kMaxSide = 64;

std::vector<uint8_t> readFile(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    return {};
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),
                              std::istreambuf_iterator<char>());
}

std::vector<uint8_t> embeddedPicture(const fs::path &track) {
  TagLib::FileRef ref(track.c_str());
  if (ref.isNull() || !ref.file())
    return {};

  const auto props = ref.file()->complexProperties("PICTURE");
  // Priorité à la face avant, sinon la première image disponible.
  for (const auto &prop : props) {
    if (prop.value("pictureType").value<TagLib::String>() == "Front Cover") {
      const TagLib::ByteVector data =
          prop.value("data").value<TagLib::ByteVector>();
      if (!data.isEmpty())
        return std::vector<uint8_t>(data.begin(), data.end());
    }
  }
  for (const auto &prop : props) {
    const TagLib::ByteVector data =
        prop.value("data").value<TagLib::ByteVector>();
    if (!data.isEmpty())
      return std::vector<uint8_t>(data.begin(), data.end());
  }
  return {};
}

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

int stemRank(const std::string &stem) {
  if (stem == "front")
    return 0;
  if (stem == "cover")
    return 1;
  if (stem == "folder")
    return 2;
  if (stem == "album")
    return 3;
  return -1;
}

// Comparaison insensible à la casse sur le nom et l'extension.
std::vector<uint8_t> folderPicture(const fs::path &track) {
  std::error_code ec;
  fs::path best;
  int bestRank = 99;
  for (const auto &entry : fs::directory_iterator(track.parent_path(), ec)) {
    if (!entry.is_regular_file(ec))
      continue;
    const std::string ext = lower(entry.path().extension().string());
    if (ext != ".jpg" && ext != ".jpeg" && ext != ".png")
      continue;
    const int rank = stemRank(lower(entry.path().stem().string()));
    if (rank >= 0 && rank < bestRank) {
      best = entry.path();
      bestRank = rank;
    }
  }
  return best.empty() ? std::vector<uint8_t>{} : readFile(best);
}

// Réduction par échantillonnage (suffisant pour 12×6 cellules).
CoverArt downscale(const uint8_t *pixels, int w, int h) {
  const double scale =
      std::min(1.0, static_cast<double>(kMaxSide) / std::max(w, h));
  const int tw = std::max(1, static_cast<int>(w * scale));
  const int th = std::max(1, static_cast<int>(h * scale));

  CoverArt out;
  out.w = tw;
  out.h = th;
  out.rgb.resize(static_cast<size_t>(tw) * th * 3);
  for (int y = 0; y < th; ++y) {
    const int sy = std::min(h - 1, y * h / th);
    for (int x = 0; x < tw; ++x) {
      const int sx = std::min(w - 1, x * w / tw);
      const uint8_t *src = pixels + (static_cast<size_t>(sy) * w + sx) * 3;
      uint8_t *dst = out.rgb.data() + (static_cast<size_t>(y) * tw + x) * 3;
      dst[0] = src[0];
      dst[1] = src[1];
      dst[2] = src[2];
    }
  }
  return out;
}

} // namespace

std::optional<CoverArt> loadCover(const fs::path &track) {
  std::vector<uint8_t> encoded = embeddedPicture(track);
  if (encoded.empty())
    encoded = folderPicture(track);
  if (encoded.empty())
    return std::nullopt;

  int w = 0, h = 0, channels = 0;
  stbi_uc *decoded = stbi_load_from_memory(
      encoded.data(), static_cast<int>(encoded.size()), &w, &h, &channels, 3);
  if (!decoded)
    return std::nullopt;

  CoverArt art = downscale(decoded, w, h);
  stbi_image_free(decoded);
  return art;
}
