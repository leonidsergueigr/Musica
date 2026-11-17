#pragma once

#include <filesystem>
#include <string>

// État persistant de l'application (dernier dossier, volume, mode de lecture,
// langue). Un fichier absent ou corrompu donne simplement les valeurs par
// défaut.
struct SavedState {
  std::string lastDir;
  float volume = 1.0f;
  bool shuffle = false;
  int repeat = 0; // 0 = off, 1 = all, 2 = one
  std::string lang;
};

// ~/.config/musica/state (ou $XDG_CONFIG_HOME/musica/state).
std::filesystem::path statePath();
SavedState loadState();
bool saveState(const SavedState &state);
