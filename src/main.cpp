#include "FileBrowser.hpp"
#include "Player.hpp"
#include "PlaylistStore.hpp"
#include "Track.hpp"
#include "UiState.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;

static std::string fmtTime(float s) {
  int t = static_cast<int>(s);
  char buf[16];
  std::snprintf(buf, sizeof buf, "%02d:%02d", t / 60, t % 60);
  return buf;
}

static std::string fmtTrackTime(int s) {
  return fmtTime(static_cast<float>(s));
}

static std::string rep(const std::string &s, int n) {
  std::string out;
  out.reserve(s.size() * static_cast<size_t>(std::max(0, n)));
  for (int i = 0; i < n; ++i)
    out += s;
  return out;
}

// Barre de progression textuelle : ━ plein, ● curseur, ─ reste.
static std::string barString(float frac, int width, bool asciiCursor = false) {
  if (width <= 0)
    return {};
  frac = std::clamp(frac, 0.0f, 1.0f);
  int pos = std::clamp(static_cast<int>(frac * (width - 1)), 0, width - 1);
  std::string out;
  for (int i = 0; i < width; ++i) {
    if (i == pos)
      out += asciiCursor ? "|" : "●";
    else if (i < pos)
      out += "━";
    else
      out += "─";
  }
  return out;
}

// Spectre multi-lignes. `rows` lignes de haut, chaque barre fait `barWidth`
// cellules. La ligne supérieure de chaque barre utilise les huit niveaux
// ▁▂▃▄▅▆▇█ pour la finesse.
static Element renderSpectrumElement(
    const std::array<float, Spectrum::kBands> &bands, int barCount, int rows,
    int barWidth, bool gap) {
  static const char *levels[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
  barCount = std::max(1, barCount);
  rows = std::max(1, rows);
  barWidth = std::max(1, barWidth);

  Elements cols;
  for (int j = 0; j < barCount; ++j) {
    const int b0 = j * static_cast<int>(Spectrum::kBands) / barCount;
    const int b1 = std::max(b0 + 1, (j + 1) * static_cast<int>(Spectrum::kBands) /
                                       barCount);
    float v = 0.0f;
    for (int k = b0; k < b1 && k < static_cast<int>(bands.size()); ++k)
      v = std::max(v, bands[k]);

    const float t = barCount > 1 ? static_cast<float>(j) / (barCount - 1) : 0.f;
    const Color c(Color::RGB(static_cast<uint8_t>(60 + t * 150),
                             static_cast<uint8_t>(210 - t * 110), 255));

    const float h = v * static_cast<float>(rows);
    const int full = std::clamp(static_cast<int>(h), 0, rows);
    const float frac = h - std::floor(h);

    Elements lines;
    for (int r = rows - 1; r >= 0; --r) {
      if (r < full) {
        lines.push_back(text(rep("█", barWidth)) | color(c));
      } else if (r == full && frac > 0.05f) {
        const int lvl = std::clamp(static_cast<int>(frac * 8.0f), 0, 7);
        lines.push_back(text(rep(levels[lvl], barWidth)) | color(c));
      } else {
        lines.push_back(text(std::string(barWidth, ' ')));
      }
    }
    cols.push_back(vbox(std::move(lines)));
    if (gap && j + 1 < barCount)
      cols.push_back(text(" "));
  }
  return hbox(std::move(cols));
}

static bool inBox(const Box &b, int x, int y) {
  return x >= b.x_min && x <= b.x_max && y >= b.y_min && y <= b.y_max;
}

static float fracInBox(const Box &b, int x) {
  const int w = b.x_max - b.x_min;
  if (w <= 0)
    return 0.0f;
  return std::clamp(static_cast<float>(x - b.x_min) / static_cast<float>(w),
                    0.0f, 1.0f);
}

// Toutes les actions déclenchables au clavier comme à la souris passent par
// ce point unique afin de ne pas dupliquer la logique.
enum class Action {
  Quit,
  TogglePlay,
  Next,
  Previous,
  SeekForward,
  SeekBackward,
  VolumeUp,
  VolumeDown,
  ToggleShuffle,
  ToggleRepeat,
  Up,
  Down,
  FocusBrowser,
  FocusQueue,
  Activate,
  AddFile,
  AddFolder,
  OpenPlaylists,
  AddToPlaylist,
};

static void moveSel(size_t &sel, int delta, size_t count) {
  if (count == 0) {
    sel = 0;
    return;
  }
  long v = static_cast<long>(sel) + delta;
  sel =
      static_cast<size_t>(std::clamp<long>(v, 0, static_cast<long>(count) - 1));
}

int main(int argc, char **argv) {
  bool noMouse = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--no-mouse")
      noMouse = true;
  }

  Player player;
  if (!player.init())
    return 1;
  FileBrowser browser;
  PlaylistStore store;
  UiState ui;
  std::mutex mtx; // protège player + browser + store + ui

  std::vector<std::string> plNames;
  std::vector<size_t> plCounts;

  // Zones cliquables mémorisées pendant le rendu (coordonnées écran).
  Box progressBox, volumeBox;
  Box prevBox, playBox, nextBox, shuffleBox, repeatBox;
  Box browserPanelBox, queuePanelBox;
  std::vector<Box> browserRowBoxes, queueRowBoxes;
  std::vector<Box> popupRowBoxes;
  Box popupPanelBox;
  bool dragProgress = false;
  bool dragVolume = false;

  // Détection du double-clic (même ligne, < 400 ms).
  Panel lastClickPanel = Panel::Browser;
  long lastClickRow = -1;
  std::chrono::steady_clock::time_point lastClick;

  auto screen = ScreenInteractive::Fullscreen();
  screen.TrackMouse(!noMouse);

  // ---------- Helpers playlists ----------
  auto refreshPlaylists = [&] {
    plNames = store.list();
    plCounts.clear();
    for (auto &n : plNames)
      plCounts.push_back(store.load(n).size());
    ui.plSel = std::min(ui.plSel, plNames.size()); // size() = ligne "Nouvelle"
  };

  auto openNameInput = [&](NameAction a, std::string prompt,
                           std::string initial) {
    ui.popup = PopupMode::NameInput;
    ui.nameAction = a;
    ui.prompt = std::move(prompt);
    ui.input = std::move(initial);
    ui.error.clear();
  };

  // ---------- Rendu ----------
  auto renderBar = [&] {
    const Track *t = player.currentTrack();
    const float pos = player.position();
    const float dur = player.duration();
    const int screenW = screen.dimx();
    const int screenH = screen.dimy();
    const bool compact = screenW < 80 || screenH < 24;

    std::string title = t ? t->title : "Rien en cours";
    std::string artist = (t && !t->artist.empty()) ? t->artist : "";
    std::string nowPlaying =
        std::string(player.isPlaying() ? "▶ " : "⏸ ") +
        (artist.empty() ? title : artist + " — " + title);

    const std::string shuffleLabel =
        player.queue().shuffle() ? "⇄ on" : "⇄ off";
    const std::string repeatLabel =
        player.queue().repeat() == RepeatMode::Off
            ? "↻ off"
            : (player.queue().repeat() == RepeatMode::All ? "↻ all" : "↻ one");
    // Spectre : nombre de barres adapté à la largeur disponible.
    const int logoW = compact ? 0 : 16;
    const int rightW = std::max(20, screenW - logoW - 4);
    const int rows = compact ? 1 : 5;
    const int barWidth = compact ? 1 : 3;
    const int step = barWidth + 1; // 3 cellules + 1 espace
    const int maxBars = compact ? std::max(8, rightW) : rightW / step;
    const int barCount = std::clamp(maxBars, compact ? 8 : 12, 32);

    Element spectrum = renderSpectrumElement(
        player.spectrum().bands(), barCount, rows, barWidth, !compact);
    if (!compact)
      spectrum = hbox({filler(), spectrum, filler()});

    const int barW = std::max(10, screenW - logoW - 4 - 14);
    Element progressBar = text(barString(dur > 0 ? pos / dur : 0.f, barW)) |
                          reflect(progressBox);
    Element volumeBar =
        text(barString(player.volume(), 10)) | reflect(volumeBox);

    Element right = vbox({
        spectrum,
        hbox({text(fmtTime(pos)) | dim, progressBar, text(" " + fmtTime(dur)) | dim}),
        text(nowPlaying) | bold | center,
        hbox({text("⏮") | reflect(prevBox), text("  "),
              text("⏯") | reflect(playBox), text("  "),
              text("⏭") | reflect(nextBox), text("   "),
              text(shuffleLabel) | reflect(shuffleBox), text("   "),
              text(repeatLabel) | reflect(repeatBox), text("   vol "),
              volumeBar,
              text(" " + std::to_string(static_cast<int>(player.volume() * 100)) +
                   "%")}) |
            center,
    });

    if (compact)
      return window(text(" Musica "), std::move(right)) | flex;

    Element logo = vbox({
        filler(),
        text("M U S I C A") | bold | color(Color::Cyan) | center,
        filler(),
    }) | size(WIDTH, EQUAL, logoW);

    return window(text(" Musica "),
                  hbox({std::move(logo), std::move(right) | flex}));
  };

  auto renderBrowser = [&] {
    const auto &es = browser.entries();
    size_t count = es.size() + 1;
    ui.browserSel = std::min(ui.browserSel, count - 1);
    bool focused = ui.focus == Panel::Browser;

    Elements rows;
    browserRowBoxes.assign(count, Box{});
    for (size_t i = 0; i < count; ++i) {
      std::string label =
          i == 0 ? "  .." : (es[i - 1].isDir ? "▸ " : "  ") + es[i - 1].name;
      Element row = text(label) | reflect(browserRowBoxes[i]);
      if (i == ui.browserSel)
        row = row | (focused ? inverted : bold) | focus;
      rows.push_back(row);
    }
    auto box = window(text(" " + browser.cwd().string() + " "),
                      vbox(std::move(rows)) | vscroll_indicator | yframe) |
               flex | reflect(browserPanelBox);
    return focused ? box | color(Color::Cyan) : box;
  };

  auto renderQueue = [&] {
    const auto &tr = player.queue().tracks();
    ui.queueSel = tr.empty() ? 0 : std::min(ui.queueSel, tr.size() - 1);
    bool focused = ui.focus == Panel::Queue;

    Elements rows;
    int total = 0;
    queueRowBoxes.assign(tr.size(), Box{});
    for (size_t i = 0; i < tr.size(); ++i) {
      total += tr[i].durationSec;
      bool cur = player.currentTrack() && i == player.queue().currentIndex();
      Element row = hbox({text(cur ? "▶ " : "  "), text(tr[i].title) | flex,
                          text(" " + fmtTrackTime(tr[i].durationSec))}) |
                    reflect(queueRowBoxes[i]);
      if (i == ui.queueSel)
        row = row | (focused ? inverted : bold) | focus;
      rows.push_back(row);
    }
    rows.push_back(filler());
    auto box =
        window(text(" File d'attente "),
               vbox({vbox(std::move(rows)) | vscroll_indicator | yframe | flex,
                     separator(),
                     text(std::to_string(tr.size()) + " pistes · " +
                          fmtTrackTime(total))})) |
        flex | reflect(queuePanelBox);
    return focused ? box | color(Color::Cyan) : box;
  };

  auto renderHelp = [&] {
    return window(
        text(" Commandes "),
        vbox({text(ui.message) | color(Color::Green),
              text("Lecture  ␣ pause/play  n suivant  b précédent  ←/→ ±5s  "
                   "+/- volume  s shuffle  r repeat"),
              text("Naviguer j/k ↕  h/l ⇆  ⏎ jouer  a +file  A +dossier  p "
                   "playlists  P +playlist  q quitter")}) |
            dim);
  };

  auto renderPopup = [&]() -> Element {
    if (ui.popup == PopupMode::NameInput) {
      Elements rows = {hbox({text("> "), text(ui.input), text("█") | blink}),
                       separator()};
      if (!ui.error.empty())
        rows.push_back(text(ui.error) | color(Color::Red));
      rows.push_back(text("⏎ valider   esc annuler") | dim);
      return window(text(" " + ui.prompt + " "), vbox(std::move(rows))) |
             size(WIDTH, EQUAL, 50) | reflect(popupPanelBox);
    }

    Elements rows;
    popupRowBoxes.assign(plNames.size() + 1, Box{});
    for (size_t i = 0; i < plNames.size(); ++i) {
      Element row = hbox({text("  " + plNames[i]) | flex,
                          text(std::to_string(plCounts[i]) + " pistes  ")}) |
                    reflect(popupRowBoxes[i]);
      if (i == ui.plSel)
        row = row | inverted;
      rows.push_back(row);
    }
    Element add = text("  + Nouvelle playlist…") |
                  reflect(popupRowBoxes[plNames.size()]);
    if (ui.plSel == plNames.size())
      add = add | inverted;
    rows.push_back(add);
    rows.push_back(separator());

    bool adding = ui.popup == PopupMode::AddTrack;
    rows.push_back(text(adding ? "⏎ ajouter ici   esc annuler"
                               : "⏎ charger  a +file  s sauver la file  r "
                                 "renommer  d suppr  esc") |
                   dim);
    return window(text(adding ? " Ajouter à une playlist " : " Playlists "),
                  vbox(std::move(rows))) |
           size(WIDTH, EQUAL, 70) | reflect(popupPanelBox);
  };

  auto root = Renderer([&]() -> Element {
    std::lock_guard<std::mutex> lk(mtx);
    Element base =
        vbox({renderBar(), hbox({renderBrowser(), renderQueue()}) | flex,
              renderHelp()});
    if (ui.popup == PopupMode::None)
      return base;
    return dbox({base, renderPopup() | clear_under | center});
  });

  // ---------- Handler du popup ----------
  auto handlePopup = [&](Event e) -> bool {
    // ---- saisie d'un nom ----
    if (ui.popup == PopupMode::NameInput) {
      if (e == Event::Escape) {
        // Retour au popup précédent selon l'action
        ui.popup = (ui.nameAction == NameAction::CreateAndAdd)
                       ? PopupMode::AddTrack
                       : PopupMode::Manage;
        return true;
      }
      if (e == Event::Backspace) {
        // Supprime un caractère UTF-8 complet
        while (!ui.input.empty() && (ui.input.back() & 0xC0) == 0x80)
          ui.input.pop_back();
        if (!ui.input.empty())
          ui.input.pop_back();
        return true;
      }
      if (e == Event::Return) {
        const std::string name = ui.input;
        bool ok = false;
        switch (ui.nameAction) {
        case NameAction::Create:
          ok = !name.empty() && store.create(name);
          break;
        case NameAction::CreateAndAdd:
          ok = !name.empty() && store.create(name) &&
               store.addTrack(name, ui.pendingTrack);
          break;
        case NameAction::Rename:
          ok = !name.empty() && store.rename(ui.target, name);
          break;
        case NameAction::SaveQueue: {
          std::vector<std::filesystem::path> ps;
          for (auto &t : player.queue().tracks())
            ps.push_back(t.path);
          ok = !name.empty() && store.save(name, ps);
          break;
        }
        }
        if (!ok) {
          ui.error = "Échec : nom vide, invalide ou déjà pris";
          return true;
        }
        if (ui.nameAction == NameAction::CreateAndAdd) {
          ui.message = "Ajouté à « " + name + " »";
          ui.popup = PopupMode::None;
        } else {
          refreshPlaylists();
          ui.popup = PopupMode::Manage;
        }
        return true;
      }
      if (e.is_character() && ui.input.size() < 40 && e.character() != "/")
        ui.input += e.character();
      return true; // le popup capture tout
    }

    // ---- souris dans le popup ----
    if (e.is_mouse()) {
      const Mouse m = e.mouse();
      if (m.motion == Mouse::Pressed && m.button == Mouse::Left) {
        if (!inBox(popupPanelBox, m.x, m.y)) {
          ui.popup = PopupMode::None; // clic hors du popup : on ferme
          return true;
        }
        for (size_t i = 0; i < popupRowBoxes.size(); ++i) {
          if (inBox(popupRowBoxes[i], m.x, m.y)) {
            ui.plSel = i;
            return true;
          }
        }
        return true;
      }
      if ((m.button == Mouse::WheelUp || m.button == Mouse::WheelDown) &&
          inBox(popupPanelBox, m.x, m.y)) {
        moveSel(ui.plSel, m.button == Mouse::WheelDown ? 1 : -1,
                plNames.size() + 1);
        return true;
      }
      return true;
    }

    // ---- liste des playlists (Manage / AddTrack) ----
    size_t rowCount = plNames.size() + 1;
    if (e == Event::Escape) {
      ui.popup = PopupMode::None;
      return true;
    }
    if (e == Event::Character('j') || e == Event::ArrowDown) {
      moveSel(ui.plSel, 1, rowCount);
      return true;
    }
    if (e == Event::Character('k') || e == Event::ArrowUp) {
      moveSel(ui.plSel, -1, rowCount);
      return true;
    }

    bool onNew = ui.plSel == plNames.size();
    bool adding = ui.popup == PopupMode::AddTrack;

    if (e == Event::Return) {
      if (onNew) {
        openNameInput(adding ? NameAction::CreateAndAdd : NameAction::Create,
                      "Nouvelle playlist", "");
      } else if (adding) {
        bool ok = store.addTrack(plNames[ui.plSel], ui.pendingTrack);
        ui.message =
            ok ? "Ajouté à « " + plNames[ui.plSel] + " »" : "Échec de l'ajout";
        ui.popup = PopupMode::None;
      } else {
        std::vector<Track> tracks;
        for (auto &p : store.load(plNames[ui.plSel]))
          if (std::filesystem::exists(p))
            tracks.push_back(readTrack(p));
        if (!tracks.empty()) {
          player.queue().replace(std::move(tracks));
          player.playQueueIndex(0);
          ui.queueSel = 0;
        }
        ui.popup = PopupMode::None;
      }
      return true;
    }

    if (adding)
      return true; // en mode ajout, seules les touches ci-dessus comptent

    // Sauver la file : disponible même sans playlist sélectionnée
    if (e == Event::Character('s')) {
      openNameInput(NameAction::SaveQueue, "Sauver la file sous le nom",
                    onNew ? "" : plNames[ui.plSel]);
      return true;
    }

    if (onNew)
      return true; // les touches ci-dessous visent une playlist existante

    const std::string name = plNames[ui.plSel];
    if (e == Event::Character('a')) {
      for (auto &p : store.load(name))
        if (std::filesystem::exists(p))
          player.enqueue(readTrack(p));
      ui.message = "Playlist ajoutée à la file";
      ui.popup = PopupMode::None;
    } else if (e == Event::Character('r')) {
      ui.target = name;
      openNameInput(NameAction::Rename, "Renommer", name);
    } else if (e == Event::Character('d')) {
      store.remove(name);
      refreshPlaylists();
    }
    return true;
  };

  // ---------- Actions ----------
  auto quit = screen.ExitLoopClosure();

  auto perform = [&](Action a) {
    const bool inBrowser = ui.focus == Panel::Browser;
    const auto &es = browser.entries();
    switch (a) {
    case Action::Quit:
      quit();
      break;
    case Action::TogglePlay:
      player.toggle();
      break;
    case Action::Next:
      player.next();
      break;
    case Action::Previous:
      player.previous();
      break;
    case Action::SeekForward:
      player.seekRelative(5);
      break;
    case Action::SeekBackward:
      player.seekRelative(-5);
      break;
    case Action::VolumeUp:
      player.setVolume(player.volume() + 0.05f);
      break;
    case Action::VolumeDown:
      player.setVolume(player.volume() - 0.05f);
      break;
    case Action::ToggleShuffle:
      player.queue().setShuffle(!player.queue().shuffle());
      break;
    case Action::ToggleRepeat: {
      const auto m = player.queue().repeat();
      player.queue().setRepeat(m == RepeatMode::Off   ? RepeatMode::All
                               : m == RepeatMode::All ? RepeatMode::One
                                                      : RepeatMode::Off);
      break;
    }
    case Action::Up:
      if (inBrowser)
        moveSel(ui.browserSel, -1, es.size() + 1);
      else
        moveSel(ui.queueSel, -1, player.queue().tracks().size());
      break;
    case Action::Down:
      if (inBrowser)
        moveSel(ui.browserSel, 1, es.size() + 1);
      else
        moveSel(ui.queueSel, 1, player.queue().tracks().size());
      break;
    case Action::FocusBrowser:
      ui.focus = Panel::Browser;
      break;
    case Action::FocusQueue:
      ui.focus = Panel::Queue;
      break;
    case Action::Activate: {
      if (!inBrowser) {
        player.playQueueIndex(ui.queueSel);
      } else if (ui.browserSel == 0) {
        browser.up();
        ui.browserSel = 0;
      } else if (ui.browserSel <= es.size()) {
        const auto &entry = es[ui.browserSel - 1];
        if (entry.isDir) {
          browser.enter(ui.browserSel - 1);
          ui.browserSel = 0;
        } else {
          // Joue ce fichier, et met tous les fichiers du dossier dans la file
          std::vector<Track> tracks;
          size_t start = 0;
          for (const auto &x : es) {
            if (x.isDir)
              continue;
            if (x.path == entry.path)
              start = tracks.size();
            tracks.push_back(readTrack(x.path));
          }
          player.queue().replace(std::move(tracks), start);
          player.playQueueIndex(start);
          ui.queueSel = start;
        }
      }
      break;
    }
    case Action::AddFile:
      if (inBrowser && ui.browserSel > 0 && ui.browserSel <= es.size() &&
          !es[ui.browserSel - 1].isDir)
        player.enqueue(readTrack(es[ui.browserSel - 1].path));
      break;
    case Action::AddFolder:
      if (inBrowser && ui.browserSel > 0 && ui.browserSel <= es.size() &&
          es[ui.browserSel - 1].isDir)
        player.enqueueAll(browser.audioFilesIn(es[ui.browserSel - 1].path));
      break;
    case Action::OpenPlaylists:
      refreshPlaylists();
      ui.plSel = 0;
      ui.popup = PopupMode::Manage;
      break;
    case Action::AddToPlaylist:
      if (inBrowser && ui.browserSel > 0 && ui.browserSel <= es.size() &&
          !es[ui.browserSel - 1].isDir) {
        ui.pendingTrack = std::filesystem::absolute(es[ui.browserSel - 1].path);
        refreshPlaylists();
        ui.plSel = 0;
        ui.popup = PopupMode::AddTrack;
      }
      break;
    }
  };

  auto clickRow = [&](Panel p, size_t idx) {
    const auto now = std::chrono::steady_clock::now();
    const bool dbl = lastClickPanel == p && lastClickRow == (long)idx &&
                     now - lastClick < std::chrono::milliseconds(400);
    if (p == Panel::Browser) {
      ui.focus = Panel::Browser;
      ui.browserSel = idx;
    } else {
      ui.focus = Panel::Queue;
      ui.queueSel = idx;
    }
    if (dbl)
      perform(Action::Activate);
    lastClickPanel = p;
    lastClickRow = (long)idx;
    lastClick = now;
  };

  root = CatchEvent(root, [&](Event e) {
    std::lock_guard<std::mutex> lk(mtx);

    if (e != Event::Custom)
      ui.message.clear();
    if (ui.popup != PopupMode::None)
      return handlePopup(e);

    // ---- Souris ----
    if (e.is_mouse()) {
      const Mouse m = e.mouse();
      if (m.motion == Mouse::Released) {
        dragProgress = false;
        dragVolume = false;
        return true;
      }
      if (m.button == Mouse::Left) {
        if (m.motion == Mouse::Pressed) {
          if (inBox(prevBox, m.x, m.y)) {
            perform(Action::Previous);
            return true;
          }
          if (inBox(playBox, m.x, m.y)) {
            perform(Action::TogglePlay);
            return true;
          }
          if (inBox(nextBox, m.x, m.y)) {
            perform(Action::Next);
            return true;
          }
          if (inBox(shuffleBox, m.x, m.y)) {
            perform(Action::ToggleShuffle);
            return true;
          }
          if (inBox(repeatBox, m.x, m.y)) {
            perform(Action::ToggleRepeat);
            return true;
          }
          if (inBox(progressBox, m.x, m.y) && player.duration() > 0) {
            dragProgress = true;
            player.seekTo(fracInBox(progressBox, m.x) * player.duration());
            return true;
          }
          if (inBox(volumeBox, m.x, m.y)) {
            dragVolume = true;
            player.setVolume(fracInBox(volumeBox, m.x));
            return true;
          }
          for (size_t i = 0; i < browserRowBoxes.size(); ++i) {
            if (inBox(browserRowBoxes[i], m.x, m.y)) {
              clickRow(Panel::Browser, i);
              return true;
            }
          }
          for (size_t i = 0; i < queueRowBoxes.size(); ++i) {
            if (inBox(queueRowBoxes[i], m.x, m.y)) {
              clickRow(Panel::Queue, i);
              return true;
            }
          }
        } else if (m.motion == Mouse::Moved) {
          if (dragProgress && player.duration() > 0) {
            player.seekTo(fracInBox(progressBox, m.x) * player.duration());
            return true;
          }
          if (dragVolume) {
            player.setVolume(fracInBox(volumeBox, m.x));
            return true;
          }
        }
      }
      if (m.button == Mouse::WheelUp || m.button == Mouse::WheelDown) {
        const int dir = m.button == Mouse::WheelDown ? 1 : -1;
        if (inBox(volumeBox, m.x, m.y)) {
          player.setVolume(player.volume() +
                           (m.button == Mouse::WheelUp ? 0.05f : -0.05f));
          return true;
        }
        if (inBox(browserPanelBox, m.x, m.y)) {
          moveSel(ui.browserSel, dir, browser.entries().size() + 1);
          return true;
        }
        if (inBox(queuePanelBox, m.x, m.y)) {
          moveSel(ui.queueSel, dir, player.queue().tracks().size());
          return true;
        }
      }
      return false;
    }

    // ---- Clavier ----
    if (e == Event::Character('q')) {
      perform(Action::Quit);
      return true;
    }
    if (e == Event::Character('j') || e == Event::ArrowDown) {
      perform(Action::Down);
      return true;
    }
    if (e == Event::Character('k') || e == Event::ArrowUp) {
      perform(Action::Up);
      return true;
    }
    if (e == Event::Character('h')) {
      perform(Action::FocusBrowser);
      return true;
    }
    if (e == Event::Character('l')) {
      perform(Action::FocusQueue);
      return true;
    }
    if (e == Event::Character('p')) {
      perform(Action::OpenPlaylists);
      return true;
    }
    if (e == Event::Character('P')) {
      perform(Action::AddToPlaylist);
      return true;
    }
    if (e == Event::Return) {
      perform(Action::Activate);
      return true;
    }
    if (e == Event::Character('a')) {
      perform(Action::AddFile);
      return true;
    }
    if (e == Event::Character('A')) {
      perform(Action::AddFolder);
      return true;
    }
    if (e == Event::Character(' ')) {
      perform(Action::TogglePlay);
      return true;
    }
    if (e == Event::Character('n')) {
      perform(Action::Next);
      return true;
    }
    if (e == Event::Character('b')) {
      perform(Action::Previous);
      return true;
    }
    if (e == Event::ArrowRight) {
      perform(Action::SeekForward);
      return true;
    }
    if (e == Event::ArrowLeft) {
      perform(Action::SeekBackward);
      return true;
    }
    if (e == Event::Character('+') || e == Event::Character('=')) {
      perform(Action::VolumeUp);
      return true;
    }
    if (e == Event::Character('-')) {
      perform(Action::VolumeDown);
      return true;
    }
    if (e == Event::Character('s')) {
      perform(Action::ToggleShuffle);
      return true;
    }
    if (e == Event::Character('r')) {
      perform(Action::ToggleRepeat);
      return true;
    }
    return false;
  });

  // ---------- Thread de rafraîchissement ----------
  std::atomic<bool> running{true};
  std::thread ticker([&] {
    while (running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      {
        std::lock_guard<std::mutex> lk(mtx);
        player.tick();
      }
      screen.PostEvent(Event::Custom); // force un redessin
    }
  });

  screen.Loop(root);

  running = false;
  ticker.join();
}
