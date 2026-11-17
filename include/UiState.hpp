#pragma once
#include "Track.hpp"
#include <cstddef>
#include <filesystem>
#include <string>

enum class Panel { Browser, Queue };
enum class PopupMode {
  None,
  Manage,
  AddTrack,
  NameInput,
  Help,
  ConfirmDelete,
  Info
};
enum class NameAction { Create, CreateAndAdd, Rename, SaveQueue };

struct UiState {
  Panel focus = Panel::Browser;
  size_t browserSel = 0;
  size_t queueSel = 0;

  PopupMode popup = PopupMode::None;
  size_t plSel = 0;
  std::string input;
  std::string prompt;
  std::string error;
  NameAction nameAction = NameAction::Create;
  std::string target;
  std::filesystem::path pendingTrack;
  Track infoTrack;

  std::string message;
  bool messageIsError = false;

  // Recherche : filtre le panneau actif, saisie manuelle.
  bool searching = false;
  std::string query;
  Panel searchPanel = Panel::Browser;
};
