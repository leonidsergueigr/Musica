#pragma once

#include <filesystem>
#include <string>

// Persistent application state (last directory, volume, playback modes,
// language). A missing or malformed file falls back to the defaults.
struct SavedState {
  std::string lastDir;
  float volume = 1.0f;
  bool shuffle = false;
  int repeat = 0; // 0 = off, 1 = all, 2 = one
  std::string lang;
};

// ~/.config/musica/state, or $XDG_CONFIG_HOME/musica/state.
std::filesystem::path statePath();
SavedState loadState();
bool saveState(const SavedState &state);
