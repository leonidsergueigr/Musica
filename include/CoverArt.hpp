#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

// Pochette décodée en RGB, réduite à une petite taille (≤ 64 px de côté).
// Ce module ne dépend pas de l'UI.
struct CoverArt {
  int w = 0;
  int h = 0;
  std::vector<uint8_t> rgb; // w * h * 3
};

// Cherche une pochette intégrée (TagLib) puis un fichier cover/folder/front/
// album (.jpg, .jpeg, .png) dans le dossier de la piste, et la décode.
std::optional<CoverArt> loadCover(const std::filesystem::path &track);
