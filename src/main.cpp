#include "FileBrowser.hpp"
#include "Player.hpp"
#include "PlaylistStore.hpp"
#include "Track.hpp"
#include "UiState.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
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

// Déplace une sélection de delta dans [0, count-1]
static void moveSel(size_t &sel, int delta, size_t count) {
  if (count == 0) {
    sel = 0;
    return;
  }
  long v = static_cast<long>(sel) + delta;
  sel =
      static_cast<size_t>(std::clamp<long>(v, 0, static_cast<long>(count) - 1));
}

int main() {
  Player player;
  if (!player.init())
    return 1;
  FileBrowser browser;
  PlaylistStore store;
  UiState ui;
  std::mutex mtx; // protège player + browser + store + ui

  std::vector<std::string> plNames;
  std::vector<size_t> plCounts;

  auto screen = ScreenInteractive::Fullscreen();

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
    float pos = player.position(), dur = player.duration();
    std::string title = t ? t->title : "Rien en cours";
    std::string artist = (t && !t->artist.empty()) ? t->artist : "—";
    std::string flags =
        std::string(player.queue().shuffle() ? "⇄ on " : "⇄ off ") +
        (player.queue().repeat() == RepeatMode::Off   ? "↻ off"
         : player.queue().repeat() == RepeatMode::All ? "↻ all"
                                                      : "↻ one");
    return window(
        text(" Lecture "),
        vbox({
            text(std::string(player.isPlaying() ? "▶ " : "⏸ ") + title) | bold,
            text("  " + artist) | dim,
            hbox({
                text(fmtTime(pos) + " "),
                gauge(dur > 0 ? pos / dur : 0.f) | flex,
                text(" " + fmtTime(dur) + "   vol " +
                     std::to_string(int(player.volume() * 100)) + "%   " +
                     flags),
            }),
        }));
  };

  auto renderBrowser = [&] {
    const auto &es = browser.entries();
    size_t count = es.size() + 1;
    ui.browserSel = std::min(ui.browserSel, count - 1);
    bool focused = ui.focus == Panel::Browser;

    Elements rows;
    for (size_t i = 0; i < count; ++i) {
      std::string label =
          i == 0 ? "  .." : (es[i - 1].isDir ? "▸ " : "  ") + es[i - 1].name;
      Element row = text(label);
      if (i == ui.browserSel)
        row = row | (focused ? inverted : bold) | focus;
      rows.push_back(row);
    }
    auto box = window(text(" " + browser.cwd().string() + " "),
                      vbox(std::move(rows)) | vscroll_indicator | yframe) |
               flex;
    return focused ? box | color(Color::Cyan) : box;
  };

  auto renderQueue = [&] {
    const auto &tr = player.queue().tracks();
    ui.queueSel = tr.empty() ? 0 : std::min(ui.queueSel, tr.size() - 1);
    bool focused = ui.focus == Panel::Queue;

    Elements rows;
    int total = 0;
    for (size_t i = 0; i < tr.size(); ++i) {
      total += tr[i].durationSec;
      bool cur = player.currentTrack() && i == player.queue().currentIndex();
      Element row = hbox({text(cur ? "▶ " : "  "), text(tr[i].title) | flex,
                          text(" " + fmtTrackTime(tr[i].durationSec))});
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
        flex;
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
             size(WIDTH, EQUAL, 50);
    }

    Elements rows;
    for (size_t i = 0; i < plNames.size(); ++i) {
      Element row = hbox({text("  " + plNames[i]) | flex,
                          text(std::to_string(plCounts[i]) + " pistes  ")});
      if (i == ui.plSel)
        row = row | inverted;
      rows.push_back(row);
    }
    Element add = text("  + Nouvelle playlist…");
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
           size(WIDTH, EQUAL, 70);
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
  auto handlePopup = [&](const Event &e) -> bool {
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

  // ---------- Événements ----------
  auto quit = screen.ExitLoopClosure();

  root = CatchEvent(root, [&](Event e) {
    std::lock_guard<std::mutex> lk(mtx);

    if (e != Event::Custom)
      ui.message.clear();
    if (ui.popup != PopupMode::None)
      return handlePopup(e);

    bool inBrowser = ui.focus == Panel::Browser;
    const auto &es = browser.entries();

    if (e == Event::Character('q')) {
      quit();
      return true;
    }

    // Navigation
    if (e == Event::Character('j') || e == Event::ArrowDown) {
      if (inBrowser)
        moveSel(ui.browserSel, 1, es.size() + 1);
      else
        moveSel(ui.queueSel, 1, player.queue().tracks().size());
      return true;
    }
    if (e == Event::Character('k') || e == Event::ArrowUp) {
      if (inBrowser)
        moveSel(ui.browserSel, -1, es.size() + 1);
      else
        moveSel(ui.queueSel, -1, player.queue().tracks().size());
      return true;
    }
    if (e == Event::Character('h')) {
      ui.focus = Panel::Browser;
      return true;
    }
    if (e == Event::Character('l')) {
      ui.focus = Panel::Queue;
      return true;
    }

    // Playlists
    if (e == Event::Character('p')) {
      refreshPlaylists();
      ui.plSel = 0;
      ui.popup = PopupMode::Manage;
      return true;
    }
    if (e == Event::Character('P') && inBrowser && ui.browserSel > 0 &&
        ui.browserSel <= es.size() && !es[ui.browserSel - 1].isDir) {
      ui.pendingTrack = std::filesystem::absolute(es[ui.browserSel - 1].path);
      refreshPlaylists();
      ui.plSel = 0;
      ui.popup = PopupMode::AddTrack;
      return true;
    }

    // ⏎
    if (e == Event::Return) {
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
      return true;
    }

    // Ajouts à la file
    if (e == Event::Character('a') && inBrowser && ui.browserSel > 0 &&
        ui.browserSel <= es.size()) {
      const auto &entry = es[ui.browserSel - 1];
      if (!entry.isDir)
        player.enqueue(readTrack(entry.path));
      return true;
    }
    if (e == Event::Character('A') && inBrowser && ui.browserSel > 0 &&
        ui.browserSel <= es.size()) {
      const auto &entry = es[ui.browserSel - 1];
      if (entry.isDir)
        player.enqueueAll(browser.audioFilesIn(entry.path));
      return true;
    }

    // Transport
    if (e == Event::Character(' ')) {
      player.toggle();
      return true;
    }
    if (e == Event::Character('n')) {
      player.next();
      return true;
    }
    if (e == Event::Character('b')) {
      player.previous();
      return true;
    }
    if (e == Event::ArrowRight) {
      player.seekRelative(5);
      return true;
    }
    if (e == Event::ArrowLeft) {
      player.seekRelative(-5);
      return true;
    }
    if (e == Event::Character('+') || e == Event::Character('=')) {
      player.setVolume(player.volume() + 0.05f);
      return true;
    }
    if (e == Event::Character('-')) {
      player.setVolume(player.volume() - 0.05f);
      return true;
    }
    if (e == Event::Character('s')) {
      player.queue().setShuffle(!player.queue().shuffle());
      return true;
    }
    if (e == Event::Character('r')) {
      auto m = player.queue().repeat();
      player.queue().setRepeat(m == RepeatMode::Off   ? RepeatMode::All
                               : m == RepeatMode::All ? RepeatMode::One
                                                      : RepeatMode::Off);
      return true;
    }
    return false;
  });

  // ---------- Thread de rafraîchissement ----------
  std::atomic<bool> running{true};
  std::thread ticker([&] {
    while (running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
