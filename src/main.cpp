#include "AudioEngine.hpp"
#include "FileBrowser.hpp"
#include "Track.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

static void printListing(const FileBrowser &browser, float volume) {
  std::cout << "\n" << browser.cwd().string() << "\n";
  std::cout << "---------------------------------\n";
  const auto &entries = browser.entries();
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto &e = entries[i];
    std::cout << "  " << i << "  " << (e.isDir ? "[d] " : "    ") << e.name
              << "\n";
  }
  std::cout << "---------------------------------\n";
  std::cout << "volume " << static_cast<int>(volume * 100) << "%\n";
  std::cout << "<num> enter dir / play track, u up, l refresh, p pause, "
               "r resume, +/- volume, q quit\n> ";
}

int main(int argc, char *argv[]) {
  FileBrowser browser(argc > 1 ? std::filesystem::path(argv[1])
                               : std::filesystem::current_path());

  AudioEngine engine;
  if (!engine.isInitialized()) {
    std::cerr << "Error to initialise engine.\n";
    return -1;
  }
  float volume = 1.0f;
  std::string current;

  printListing(browser, volume);
  std::string cmd;
  while (std::cin >> cmd && cmd != "q") {
    if (cmd == "u") {
      browser.up();
    } else if (cmd == "l") {
      browser.refresh();
    } else if (cmd == "p") {
      engine.pause();
      std::cout << "paused\n";
    } else if (cmd == "r") {
      engine.play();
      std::cout << "resumed\n";
    } else if (cmd == "+") {
      volume = std::min(1.0f, volume + 0.1f);
      engine.setVolume(volume);
    } else if (cmd == "-") {
      volume = std::max(0.0f, volume - 0.1f);
      engine.setVolume(volume);
    } else {
      size_t index;
      try {
        index = static_cast<size_t>(std::stoul(cmd));
      } catch (...) {
        std::cout << "unknown command\n> ";
        continue;
      }
      const auto &entries = browser.entries();
      if (index >= entries.size()) {
        std::cout << "index out of range\n> ";
        continue;
      }
      if (entries[index].isDir) {
        browser.enter(index);
      } else {
        Track t = readTrack(entries[index].path);
        if (!engine.load(t.path.string())) {
          std::cout << "Impossible to load file: " << t.path.string() << "\n";
        } else {
          engine.setVolume(volume);
          engine.play();
          current = t.title;
          std::cout << "playing: " << t.title << " - " << t.artist << " ["
                    << t.album << "] " << t.durationSec << "s, " << t.bitrate
                    << " kbps\n";
        }
      }
    }
    if (!current.empty())
      std::cout << "now playing: " << current << " ("
                << static_cast<int>(engine.getCurrentPosition()) << "s)\n";
    printListing(browser, volume);
  }
  return 0;
}
