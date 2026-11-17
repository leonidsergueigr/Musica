#include "CoverArt.hpp"
#include "FileBrowser.hpp"
#include "I18n.hpp"
#include "Player.hpp"
#include "PlaylistStore.hpp"
#include "Track.hpp"
#include "UiState.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifndef MUSICA_VERSION
#define MUSICA_VERSION "dev"
#endif

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

static void replaceAll(std::string &s, const std::string &from,
                       const std::string &to) {
  if (from.empty())
    return;
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

// Repli casse + accents pour une recherche tolérante.
static std::string foldText(const std::string &in) {
  std::string s = in;
  for (char &c : s)
    if (static_cast<unsigned char>(c) < 0x80)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  static const char *pairs[][2] = {
      {"à", "a"}, {"á", "a"}, {"â", "a"}, {"ä", "a"}, {"ã", "a"},
      {"å", "a"}, {"À", "a"}, {"Á", "a"}, {"Â", "a"}, {"Ä", "a"},
      {"é", "e"}, {"è", "e"}, {"ê", "e"}, {"ë", "e"}, {"É", "e"},
      {"È", "e"}, {"Ê", "e"}, {"Ë", "e"}, {"î", "i"}, {"ï", "i"},
      {"í", "i"}, {"Î", "i"}, {"Ï", "i"}, {"ô", "o"}, {"ö", "o"},
      {"ó", "o"}, {"õ", "o"}, {"Ô", "o"}, {"Ö", "o"}, {"û", "u"},
      {"ù", "u"}, {"ü", "u"}, {"ú", "u"}, {"Û", "u"}, {"Ù", "u"},
      {"Ü", "u"}, {"ç", "c"}, {"Ç", "c"}, {"ñ", "n"}, {"Ñ", "n"},
      {"œ", "oe"}, {"Œ", "oe"}, {"æ", "ae"}, {"Æ", "ae"}, {"ÿ", "y"},
  };
  for (const auto &p : pairs)
    replaceAll(s, p[0], p[1]);
  return s;
}

static bool containsFold(const std::string &hay, const std::string &needle) {
  if (needle.empty())
    return true;
  return foldText(hay).find(foldText(needle)) != std::string::npos;
}

// Rendu en demi-blocs : une cellule porte deux pixels verticaux.
static Element renderCover(const CoverArt &art, int cellsW, int cellsH) {
  if (art.w <= 0 || art.h <= 0 ||
      art.rgb.size() < static_cast<size_t>(art.w) * art.h * 3)
    return text("");

  Elements rows;
  rows.reserve(cellsH);
  for (int r = 0; r < cellsH; ++r) {
    Elements cells;
    cells.reserve(cellsW);
    for (int c = 0; c < cellsW; ++c) {
      const int sx = std::clamp(c * art.w / cellsW, 0, art.w - 1);
      auto pixel = [&](int py) {
        const int sy = std::clamp(py * art.h / (2 * cellsH), 0, art.h - 1);
        const uint8_t *p =
            art.rgb.data() + (static_cast<size_t>(sy) * art.w + sx) * 3;
        return Color::RGB(p[0], p[1], p[2]);
      };
      const Color top = pixel(2 * r);
      const Color bottom = pixel(2 * r + 1);
      cells.push_back(text("▀") | color(top) | bgcolor(bottom));
    }
    rows.push_back(hbox(std::move(cells)));
  }
  return vbox(std::move(rows));
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
  ToggleHelp,
  ToggleLanguage,
  StartSearch,
  ValidateSearch,
  ClearSearch,
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
  bool showHelp = false;
  bool showVersion = false;
  Lang lang = detectLang();

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--no-mouse") {
      noMouse = true;
    } else if (arg == "--lang" && i + 1 < argc) {
      lang = std::string(argv[++i]) == "fr" ? Lang::Fr : Lang::En;
    } else if (arg.rfind("--lang=", 0) == 0) {
      lang = arg.substr(7) == "fr" ? Lang::Fr : Lang::En;
    } else if (arg == "--help" || arg == "-h") {
      showHelp = true;
    } else if (arg == "--version" || arg == "-v") {
      showVersion = true;
    }
  }
  setLang(lang);

  if (showVersion) {
    std::printf("musica %s\n", MUSICA_VERSION);
    return 0;
  }
  if (showHelp) {
    std::printf("%s %s\n\n%s\n\n%s\n", tr(Str::AppTitle), MUSICA_VERSION,
                tr(Str::CliUsage), tr(Str::CliOptions));
    std::printf("  --no-mouse     %s\n", tr(Str::CliNoMouse));
    std::printf("  --lang fr|en   %s\n", tr(Str::CliLang));
    std::printf("  --help         %s\n", tr(Str::CliHelp));
    std::printf("  --version      %s\n", tr(Str::CliVersion));
    return 0;
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
  std::function<void(Action)> performFn;

  // Pochette : chargée hors des verrous principaux par un thread dédié, mise
  // en cache par album/dossier. Toutes ces variables sont protégées par
  // coverMutex uniquement (ordre autorisé : mtx puis coverMutex).
  std::mutex coverMutex;
  std::unordered_map<std::string, std::shared_ptr<const CoverArt>> coverCache;
  std::shared_ptr<const CoverArt> currentCover;
  std::string requestedCoverKey;
  std::condition_variable coverCv;
  std::string pendingCoverKey;
  std::filesystem::path pendingCoverPath;
  bool coverPending = false;
  bool coverStop = false;

  std::vector<std::pair<Box, std::function<void()>>> chipHandlers;
  std::vector<std::pair<Box, Event>> popupChipBoxes;
  std::function<bool(Event)> handlePopup;

  // Détection du double-clic (même ligne, < 400 ms).
  Panel lastClickPanel = Panel::Browser;
  long lastClickRow = -1;
  std::chrono::steady_clock::time_point lastClick;

  auto screen = ScreenInteractive::Fullscreen();
  screen.TrackMouse(!noMouse);

  // Cache de tags : évite de relire tout un dossier à chaque lecture.
  struct CachedTrack {
    Track track;
    std::filesystem::file_time_type mtime;
  };
  std::unordered_map<std::string, CachedTrack> trackCache;
  auto cachedTrack = [&](const std::filesystem::path &p) -> Track {
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(p, ec);
    const std::string key = p.string();
    const auto it = trackCache.find(key);
    if (it != trackCache.end() && !ec && it->second.mtime == mtime)
      return it->second.track;
    Track track = readTrack(p);
    if (!ec) {
      if (trackCache.size() > 4096)
        trackCache.clear();
      trackCache[key] = {track, mtime};
    }
    return track;
  };

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

  // ---------- Recherche (panneau actif) ----------
  auto clearSearch = [&] {
    ui.searching = false;
    ui.query.clear();
  };
  auto browserVisible = [&] {
    std::vector<size_t> out;
    const auto &es = browser.entries();
    for (size_t i = 0; i < es.size(); ++i) {
      const bool filtered = ui.searchPanel == Panel::Browser &&
                            !ui.query.empty();
      if (!filtered || containsFold(es[i].name, ui.query))
        out.push_back(i);
    }
    return out;
  };
  auto queueVisible = [&] {
    std::vector<size_t> out;
    const auto &ts = player.queue().tracks();
    for (size_t i = 0; i < ts.size(); ++i) {
      const bool filtered =
          ui.searchPanel == Panel::Queue && !ui.query.empty();
      if (!filtered ||
          containsFold(ts[i].title + " " + ts[i].artist + " " + ts[i].album,
                       ui.query))
        out.push_back(i);
    }
    return out;
  };

  // Puces de la barre de commandes : touche encadrée + libellé.
  auto chipElement = [](const std::string &key, const std::string &label,
                        const Color &c) {
    return hbox({text(" " + key + " ") | bgcolor(c) | color(Color::Black) |
                     bold,
                 text(" " + label)});
  };

  // ---------- Rendu ----------
  auto renderBar = [&] {
    const Track *t = player.currentTrack();
    const float pos = player.position();
    const float dur = player.duration();
    const int screenW = screen.dimx();
    const int screenH = screen.dimy();
    const bool compact = screenW < 80 || screenH < 24;

    std::string title = t ? t->title : tr(Str::NowPlayingNone);
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
    // Pochette : masquée sur les terminaux étroits, avec placeholder stable.
    const bool showCover = !compact && screenW >= 100;
    constexpr int coverW = 12;
    constexpr int coverH = 6;
    const int logoW = compact ? 0 : 16;
    const int leftW = logoW + (showCover ? coverW + 1 : 0);

    // Spectre : nombre de barres adapté à la largeur disponible.
    const int rightW = std::max(20, screenW - leftW - 4);
    const int rows = compact ? 1 : 5;
    const int barWidth = compact ? 1 : 3;
    const int step = barWidth + 1; // 3 cellules + 1 espace
    const int maxBars = compact ? std::max(8, rightW) : rightW / step;
    const int barCount = std::clamp(maxBars, compact ? 8 : 12, 32);

    Element spectrum = renderSpectrumElement(
        player.spectrum().bands(), barCount, rows, barWidth, !compact);
    if (!compact)
      spectrum = hbox({filler(), spectrum, filler()});

    const int barW = std::max(10, screenW - leftW - 4 - 14);
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
              text(repeatLabel) | reflect(repeatBox),
              text("   " + std::string(tr(Str::VolumeLabel)) + " "),
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

    Element leftBox = std::move(logo);
    if (showCover) {
      std::shared_ptr<const CoverArt> art;
      {
        std::lock_guard<std::mutex> lk(coverMutex);
        art = currentCover;
      }
      Element cover = art
          ? renderCover(*art, coverW, coverH)
          : vbox({filler(), text("♪") | center | dim, filler()}) |
                size(WIDTH, EQUAL, coverW) | size(HEIGHT, EQUAL, coverH);
      leftBox = hbox({
          vbox({filler(), std::move(cover), filler()}),
          text(" "),
          std::move(leftBox),
      }) | size(WIDTH, EQUAL, leftW);
    }

    return window(text(" Musica "),
                  hbox({std::move(leftBox), std::move(right) | flex}));
  };

  auto renderBrowser = [&] {
    const auto &es = browser.entries();
    const auto vis = browserVisible();
    size_t count = vis.size() + 1;
    ui.browserSel = std::min(ui.browserSel, count - 1);
    bool focused = ui.focus == Panel::Browser;

    Elements rows;
    browserRowBoxes.assign(count, Box{});
    for (size_t i = 0; i < count; ++i) {
      std::string label = "  ..";
      if (i > 0) {
        const auto &e = es[vis[i - 1]];
        label = (e.isDir ? "▸ " : "  ") + e.name;
      }
      Element row = text(label) | reflect(browserRowBoxes[i]);
      if (i == ui.browserSel)
        row = row | (focused ? inverted : bold) | focus;
      rows.push_back(row);
    }

    std::string titleText = " " + browser.cwd().string() + " ";
    if (ui.searchPanel == Panel::Browser && !ui.query.empty()) {
      titleText =
          " " + trf(vis.size() == 1 ? Str::FilterIndicatorOne
                                    : Str::FilterIndicator,
                    {ui.query, std::to_string(vis.size())}) +
          " ";
    }
    auto box = window(text(titleText),
                      vbox(std::move(rows)) | vscroll_indicator | yframe) |
               flex | reflect(browserPanelBox);
    return focused ? box | color(Color::Cyan) : box;
  };

  auto renderQueue = [&] {
    const auto &tracks = player.queue().tracks();
    const auto vis = queueVisible();
    ui.queueSel = vis.empty() ? 0 : std::min(ui.queueSel, vis.size() - 1);
    bool focused = ui.focus == Panel::Queue;

    Elements rows;
    int total = 0;
    for (const auto &t : tracks)
      total += t.durationSec;
    queueRowBoxes.assign(vis.size(), Box{});
    for (size_t i = 0; i < vis.size(); ++i) {
      const Track &t = tracks[vis[i]];
      bool cur = player.currentTrack() && vis[i] == player.queue().currentIndex();
      Element row = hbox({text(cur ? "▶ " : "  "), text(t.title) | flex,
                          text(" " + fmtTrackTime(t.durationSec))}) |
                    reflect(queueRowBoxes[i]);
      if (i == ui.queueSel)
        row = row | (focused ? inverted : bold) | focus;
      rows.push_back(row);
    }
    rows.push_back(filler());

    std::string queueTitle = " " + std::string(tr(Str::QueueTitle)) + " ";
    if (ui.searchPanel == Panel::Queue && !ui.query.empty()) {
      queueTitle =
          " " + trf(vis.size() == 1 ? Str::FilterIndicatorOne
                                    : Str::FilterIndicator,
                    {ui.query, std::to_string(vis.size())}) +
          " ";
    }
    auto box =
        window(text(queueTitle),
               vbox({vbox(std::move(rows)) | vscroll_indicator | yframe | flex,
                     separator(),
                     text(trf(tracks.size() == 1 ? Str::TrackCountOne
                                                : Str::TracksCount,
                              {std::to_string(tracks.size())}) +
                          " · " + fmtTrackTime(total))})) |
        flex | reflect(queuePanelBox);
    return focused ? box | color(Color::Cyan) : box;
  };

  auto renderCommands = [&] {
    chipHandlers.clear();
    popupChipBoxes.clear();
    chipHandlers.reserve(64);
    popupChipBoxes.reserve(16);

    const Color cPlay = Color::Cyan;
    const Color cNav = Color::Green;
    const Color cPl = Color::Magenta;

    struct ChipSpec {
      std::string key, label;
      Color color = Color::Cyan;
      Action action = Action::TogglePlay;
      Event ev;
      bool useEvent = false;
    };
    std::vector<ChipSpec> specs;
    auto act = [&](std::string k, std::string l, Color c, Action a) {
      ChipSpec s;
      s.key = std::move(k);
      s.label = std::move(l);
      s.color = c;
      s.action = a;
      specs.push_back(std::move(s));
    };
    auto evt = [&](std::string k, std::string l, Color c, Event e) {
      ChipSpec s;
      s.key = std::move(k);
      s.label = std::move(l);
      s.color = c;
      s.ev = std::move(e);
      s.useEvent = true;
      specs.push_back(std::move(s));
    };
    auto compactSet = [&] {
      specs.clear();
      act("␣", tr(Str::Pause), cPlay, Action::TogglePlay);
      act("n", tr(Str::Next), cPlay, Action::Next);
      act("/", tr(Str::SearchChip), cNav, Action::StartSearch);
      act("⏎", tr(Str::Play), cNav, Action::Activate);
      act("?", tr(Str::HelpChip), cPl, Action::ToggleHelp);
    };

    if (ui.popup == PopupMode::NameInput) {
      evt("⏎", tr(Str::Validate), cPlay, Event::Return);
      evt("esc", tr(Str::Cancel), cNav, Event::Escape);
    } else if (ui.popup == PopupMode::ConfirmDelete) {
      evt("y", tr(Str::Delete), cPl, Event::Character('y'));
      evt("n", tr(Str::Cancel), cNav, Event::Character('n'));
    } else if (ui.popup == PopupMode::Help) {
      evt("esc", tr(Str::CloseHelp), cNav, Event::Escape);
      evt("?", tr(Str::CloseHelp), cNav, Event::Character('?'));
    } else if (ui.popup == PopupMode::Manage) {
      evt("⏎", tr(Str::LoadChip), cPlay, Event::Return);
      evt("a", tr(Str::AddFile), cPl, Event::Character('a'));
      evt("s", tr(Str::SaveQueue), cPl, Event::Character('s'));
      evt("r", tr(Str::Rename), cPl, Event::Character('r'));
      evt("d", tr(Str::Delete), cPl, Event::Character('d'));
      evt("esc", tr(Str::Close), cNav, Event::Escape);
    } else if (ui.popup == PopupMode::AddTrack) {
      evt("⏎", tr(Str::AddHere), cPlay, Event::Return);
      evt("esc", tr(Str::Cancel), cNav, Event::Escape);
    } else if (ui.searching) {
      act("⏎", tr(Str::Validate), cPlay, Action::ValidateSearch);
      act("esc", tr(Str::Cancel), cNav, Action::ClearSearch);
    } else {
      act("␣", tr(Str::Pause), cPlay, Action::TogglePlay);
      act("n", tr(Str::Next), cPlay, Action::Next);
      act("b", tr(Str::Previous), cPlay, Action::Previous);
      act("←", tr(Str::SeekBack), cNav, Action::SeekBackward);
      act("→", tr(Str::SeekFwd), cNav, Action::SeekForward);
      act("j/k", tr(Str::Navigate), cNav, Action::Down);
      act("/", tr(Str::SearchChip), cNav, Action::StartSearch);
      act("a", tr(Str::AddFile), cPl, Action::AddFile);
      act("p", tr(Str::PlaylistsChip), cPl, Action::OpenPlaylists);
      act("?", tr(Str::HelpChip), cPl, Action::ToggleHelp);

      int total = 0;
      for (const auto &s : specs)
        total += string_width(s.key) + string_width(s.label) + 3 + (total ? 2 : 0);
      if (total > screen.dimx() - 4)
        compactSet();
    }

    Elements row;
    bool first = true;
    for (const auto &s : specs) {
      if (!first)
        row.push_back(text("  "));
      first = false;
      if (s.useEvent) {
        popupChipBoxes.emplace_back();
        Box &b = popupChipBoxes.back().first;
        popupChipBoxes.back().second = s.ev;
        row.push_back(chipElement(s.key, s.label, s.color) | reflect(b));
      } else {
        chipHandlers.emplace_back();
        Box &b = chipHandlers.back().first;
        const Action a = s.action;
        chipHandlers.back().second = [&performFn, a] { performFn(a); };
        row.push_back(chipElement(s.key, s.label, s.color) | reflect(b));
      }
    }

    Element msg = text(" ");
    if (!ui.error.empty())
      msg = text(" " + ui.error) | color(Color::Red) | bold;
    else if (!ui.message.empty())
      msg = text(" " + ui.message) |
            color(ui.messageIsError ? Color::Red : Color::Green) | bold;

    Elements content;
    if (ui.searching)
      content.push_back(hbox({text(" / "), text(ui.query),
                              text("█") | blink}));
    content.push_back(msg);
    content.push_back(hbox(std::move(row)));

    return window(text(" " + std::string(tr(Str::CommandsTitle)) + " "),
                  vbox(std::move(content)));
  };

  auto renderPopup = [&]() -> Element {
    if (ui.popup == PopupMode::Help) {
      auto key = [&](const std::string &k, const std::string &l) {
        return hbox({text(" " + k + " ") | color(Color::Cyan) | bold,
                     text(" " + l)});
      };
      Element left = vbox({
          text(tr(Str::HelpPlayback)) | bold | color(Color::Cyan),
          key("␣", tr(Str::HkPause)),
          key("n / b", tr(Str::HkNextPrev)),
          key("← / →", tr(Str::HkSeek)),
          key("+ / -", tr(Str::HkVolume)),
          key("s / r", tr(Str::HkShuffleRepeat)),
          text(""),
          text(tr(Str::HelpNav)) | bold | color(Color::Green),
          key("j / k", tr(Str::HkMove)),
          key("h / l", tr(Str::HkPanels)),
          key("⏎", tr(Str::HkEnter)),
          key("a / A", tr(Str::HkAdd)),
          key("/", tr(Str::HkSearch)),
          key("L", tr(Str::LanguageChip)),
      });
      Element right = vbox({
          text(tr(Str::PlaylistsTitle)) | bold | color(Color::Magenta),
          key("p", tr(Str::HkPlManage)),
          key("P", tr(Str::HkPlAdd)),
          key("⏎ a s r d", tr(Str::HkPlPopup)),
          key("x J / K", tr(Str::HkPlEdit)),
          key("i", tr(Str::HkInfo)),
          text(""),
          text(tr(Str::HelpMouse)) | bold | color(Color::Yellow),
          key("clic", tr(Str::HkClick)),
          key("double-clic", tr(Str::HkDouble)),
          key("molette", tr(Str::HkWheel)),
          key("glisser", tr(Str::HkDrag)),
          text(""),
          text(tr(Str::HkClose)) | dim,
      });
      return window(text(" " + std::string(tr(Str::HelpTitle)) + " "),
                    hbox({left, text("   "), right})) |
             reflect(popupPanelBox);
    }

    if (ui.popup == PopupMode::ConfirmDelete) {
      return window(
                 text(" " + std::string(tr(Str::ConfirmTitle)) + " "),
                 vbox({text(trf(Str::ConfirmDelete, {ui.target})),
                       text(std::string("  ") + tr(Str::YesNo)) | dim})) |
             size(WIDTH, EQUAL, 40) | reflect(popupPanelBox);
    }

    if (ui.popup == PopupMode::NameInput) {
      Elements rows = {hbox({text("> "), text(ui.input), text("█") | blink}),
                       separator()};
      if (!ui.error.empty())
        rows.push_back(text(ui.error) | color(Color::Red));
      return window(text(" " + ui.prompt + " "), vbox(std::move(rows))) |
             size(WIDTH, EQUAL, 50) | reflect(popupPanelBox);
    }

    Elements rows;
    popupRowBoxes.assign(plNames.size() + 1, Box{});
    for (size_t i = 0; i < plNames.size(); ++i) {
      Element row = hbox({text("  " + plNames[i]) | flex,
                          text(trf(plCounts[i] == 1 ? Str::TrackCountOne
                                                   : Str::TracksCount,
                                   {std::to_string(plCounts[i])}) +
                               "  ")}) |
                    reflect(popupRowBoxes[i]);
      if (i == ui.plSel)
        row = row | inverted;
      rows.push_back(row);
    }
    Element add = text("  " + std::string(tr(Str::NewPlaylistItem))) |
                  reflect(popupRowBoxes[plNames.size()]);
    if (ui.plSel == plNames.size())
      add = add | inverted;
    rows.push_back(add);
    rows.push_back(separator());

    bool adding = ui.popup == PopupMode::AddTrack;
    return window(text(adding
                            ? " " + std::string(tr(Str::AddToPlaylistTitle)) + " "
                            : " " + std::string(tr(Str::PlaylistsTitle)) + " "),
                  vbox(std::move(rows))) |
           size(WIDTH, EQUAL, 70) | reflect(popupPanelBox);
  };

  auto root = Renderer([&]() -> Element {
    std::lock_guard<std::mutex> lk(mtx);
    Element base =
        vbox({renderBar(), hbox({renderBrowser(), renderQueue()}) | flex,
              renderCommands()});
    if (ui.popup == PopupMode::None)
      return base;
    return dbox({base, renderPopup() | clear_under | center});
  });

  // ---------- Handler du popup ----------
  handlePopup = [&](Event e) -> bool {
    if (ui.popup == PopupMode::Help) {
      if (e == Event::Escape || e == Event::Character('?') ||
          e == Event::Character('q'))
        ui.popup = PopupMode::None;
      return true;
    }

    if (ui.popup == PopupMode::ConfirmDelete) {
      if (e == Event::Character('y') || e == Event::Character('o')) {
        store.remove(ui.target);
        refreshPlaylists();
        ui.message = trf(Str::MsgRemoved, {ui.target});
        ui.messageIsError = false;
        ui.popup = PopupMode::None;
      } else if (e == Event::Escape || e == Event::Character('n') ||
                 e == Event::Character('q')) {
        ui.popup = PopupMode::None;
      }
      return true;
    }

    // ---- puces cliquables du popup ----
    if (e.is_mouse()) {
      const Mouse m = e.mouse();
      if (m.motion == Mouse::Pressed && m.button == Mouse::Left) {
        for (auto &[box, ev] : popupChipBoxes)
          if (inBox(box, m.x, m.y))
            return handlePopup(ev);
      }
    }

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
          ui.error = tr(Str::ErrInvalidName);
          return true;
        }
        if (ui.nameAction == NameAction::CreateAndAdd) {
          ui.message = trf(Str::MsgAddedTo, {name});
          ui.popup = PopupMode::None;
        } else {
          refreshPlaylists();
          if (ui.nameAction == NameAction::Rename)
            ui.message = trf(Str::MsgRenamed, {name});
          else if (ui.nameAction == NameAction::SaveQueue)
            ui.message = trf(Str::MsgQueueSaved, {name});
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
                      tr(Str::PromptNewPlaylist), "");
      } else if (adding) {
        bool ok = store.addTrack(plNames[ui.plSel], ui.pendingTrack);
        ui.message =
            ok ? trf(Str::MsgAddedTo, {plNames[ui.plSel]})
               : std::string(tr(Str::ErrAddFailed));
        ui.popup = PopupMode::None;
      } else {
        std::vector<Track> tracks;
        for (auto &p : store.load(plNames[ui.plSel]))
          if (std::filesystem::exists(p))
            tracks.push_back(cachedTrack(p));
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
      openNameInput(NameAction::SaveQueue, tr(Str::PromptSaveQueue),
                    onNew ? "" : plNames[ui.plSel]);
      return true;
    }

    if (onNew)
      return true; // les touches ci-dessous visent une playlist existante

    const std::string name = plNames[ui.plSel];
    if (e == Event::Character('a')) {
      for (auto &p : store.load(name))
        if (std::filesystem::exists(p))
          player.enqueue(cachedTrack(p));
      ui.message = tr(Str::MsgPlaylistQueued);
      ui.popup = PopupMode::None;
    } else if (e == Event::Character('r')) {
      ui.target = name;
      openNameInput(NameAction::Rename, tr(Str::PromptRename), name);
    } else if (e == Event::Character('d')) {
      ui.target = name;
      ui.popup = PopupMode::ConfirmDelete;
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
        moveSel(ui.browserSel, -1, browserVisible().size() + 1);
      else
        moveSel(ui.queueSel, -1, queueVisible().size());
      break;
    case Action::Down:
      if (inBrowser)
        moveSel(ui.browserSel, 1, browserVisible().size() + 1);
      else
        moveSel(ui.queueSel, 1, queueVisible().size());
      break;
    case Action::FocusBrowser:
      ui.focus = Panel::Browser;
      break;
    case Action::FocusQueue:
      ui.focus = Panel::Queue;
      break;
    case Action::Activate: {
      const auto vis = inBrowser ? browserVisible() : queueVisible();
      if (!inBrowser) {
        if (ui.queueSel < vis.size())
          player.playQueueIndex(vis[ui.queueSel]);
      } else if (ui.browserSel == 0) {
        browser.up();
        ui.browserSel = 0;
        clearSearch();
      } else if (ui.browserSel <= vis.size()) {
        const auto &entry = es[vis[ui.browserSel - 1]];
        if (entry.isDir) {
          browser.enter(vis[ui.browserSel - 1]);
          ui.browserSel = 0;
          clearSearch();
        } else {
          // Joue ce fichier, et met tous les fichiers du dossier dans la file
          std::vector<Track> tracks;
          size_t start = 0;
          for (const auto &x : es) {
            if (x.isDir)
              continue;
            if (x.path == entry.path)
              start = tracks.size();
            tracks.push_back(cachedTrack(x.path));
          }
          player.queue().replace(std::move(tracks), start);
          player.playQueueIndex(start);
          ui.queueSel = start;
        }
      }
      break;
    }
    case Action::AddFile: {
      const auto vis = browserVisible();
      if (inBrowser && ui.browserSel > 0 && ui.browserSel <= vis.size() &&
          !es[vis[ui.browserSel - 1]].isDir)
        player.enqueue(cachedTrack(es[vis[ui.browserSel - 1]].path));
      break;
    }
    case Action::AddFolder: {
      const auto vis = browserVisible();
      if (inBrowser && ui.browserSel > 0 && ui.browserSel <= vis.size() &&
          es[vis[ui.browserSel - 1]].isDir)
        for (const auto &p : browser.audioFilesIn(es[vis[ui.browserSel - 1]].path))
          player.enqueue(cachedTrack(p));
      break;
    }
    case Action::OpenPlaylists:
      refreshPlaylists();
      ui.plSel = 0;
      ui.popup = PopupMode::Manage;
      break;
    case Action::AddToPlaylist: {
      const auto vis = browserVisible();
      if (inBrowser && ui.browserSel > 0 && ui.browserSel <= vis.size() &&
          !es[vis[ui.browserSel - 1]].isDir) {
        ui.pendingTrack =
            std::filesystem::absolute(es[vis[ui.browserSel - 1]].path);
        refreshPlaylists();
        ui.plSel = 0;
        ui.popup = PopupMode::AddTrack;
      }
      break;
    }
    case Action::StartSearch:
      ui.searchPanel = ui.focus;
      ui.query.clear();
      ui.searching = true;
      break;
    case Action::ValidateSearch:
      ui.searching = false;
      break;
    case Action::ClearSearch:
      clearSearch();
      break;
    case Action::ToggleHelp:
      ui.popup = ui.popup == PopupMode::Help ? PopupMode::None : PopupMode::Help;
      break;
    case Action::ToggleLanguage:
      setLang(currentLang() == Lang::En ? Lang::Fr : Lang::En);
      ui.message =
          currentLang() == Lang::Fr ? tr(Str::MsgLangFr) : tr(Str::MsgLangEn);
      break;
    }
  };
  performFn = perform;

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
      if (m.motion == Mouse::Pressed && m.button == Mouse::Left) {
        for (auto &[box, fn] : chipHandlers) {
          if (inBox(box, m.x, m.y)) {
            fn();
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

    // ---- Saisie de la recherche ----
    if (ui.searching) {
      if (e == Event::Escape) {
        perform(Action::ClearSearch);
        return true;
      }
      if (e == Event::Return) {
        perform(Action::ValidateSearch);
        return true;
      }
      if (e == Event::Backspace) {
        while (!ui.query.empty() && (ui.query.back() & 0xC0) == 0x80)
          ui.query.pop_back();
        if (!ui.query.empty())
          ui.query.pop_back();
        return true;
      }
      if (e.is_character()) {
        if (ui.query.size() < 80)
          ui.query += e.character();
        return true;
      }
      return true; // la saisie capture les autres touches
    }
    if (e == Event::Escape && !ui.query.empty()) {
      perform(Action::ClearSearch);
      return true;
    }
    if (e == Event::Character('/')) {
      perform(Action::StartSearch);
      return true;
    }

    // ---- Clavier ----
    if (e == Event::Character('?')) {
      perform(Action::ToggleHelp);
      return true;
    }
    if (e == Event::Character('L')) {
      perform(Action::ToggleLanguage);
      return true;
    }
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

  // ---------- Thread de chargement des pochettes ----------
  std::thread coverThread([&] {
    std::unique_lock<std::mutex> lk(coverMutex);
    while (true) {
      coverCv.wait(lk, [&] { return coverPending || coverStop; });
      if (coverStop)
        break;
      const std::string key = pendingCoverKey;
      const std::filesystem::path path = pendingCoverPath;
      coverPending = false;

      std::shared_ptr<const CoverArt> art;
      const auto it = coverCache.find(key);
      if (it != coverCache.end()) {
        art = it->second; // déjà chargée (ou absence déjà mémorisée)
      } else {
        lk.unlock(); // décodage sans aucun verrou
        auto loaded = loadCover(path);
        lk.lock();
        if (loaded)
          art = std::make_shared<const CoverArt>(std::move(*loaded));
        coverCache[key] = art;
      }
      // Ignore un résultat devenu obsolète pendant le décodage.
      if (key == requestedCoverKey)
        currentCover = art;
    }
  });

  // ---------- Thread de rafraîchissement ----------
  std::atomic<bool> running{true};
  std::thread ticker([&] {
    while (running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      {
        std::lock_guard<std::mutex> lk(mtx);
        player.tick();

        int skipped = 0;
        if (player.takeLoadError(skipped)) {
          ui.message = skipped > 0
              ? trf(Str::MsgSkipped, {std::to_string(skipped)})
              : std::string(tr(Str::ErrNoPlayable));
          ui.messageIsError = true;
        }

        const Track *track = player.currentTrack();
        const std::string key =
            track ? track->path.parent_path().string() + "|" + track->album
                  : std::string();
        bool notify = false;
        {
          std::lock_guard<std::mutex> coverLk(coverMutex);
          if (key != requestedCoverKey) {
            requestedCoverKey = key;
            currentCover.reset();
            if (track) {
              pendingCoverKey = key;
              pendingCoverPath = track->path;
              coverPending = true;
              notify = true;
            }
          }
        }
        if (notify)
          coverCv.notify_one();
      }
      screen.PostEvent(Event::Custom); // force un redessin
    }
  });

  screen.Loop(root);

  running = false;
  ticker.join();
  {
    std::lock_guard<std::mutex> lk(coverMutex);
    coverStop = true;
  }
  coverCv.notify_all();
  coverThread.join();
}
