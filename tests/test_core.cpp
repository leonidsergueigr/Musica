#include "Player.hpp"
#include "PlaylistStore.hpp"
#include "Queue.hpp"
#include "Spectrum.hpp"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/resource.h>
#include <vector>

namespace fs = std::filesystem;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
      return false;                                                            \
    }                                                                          \
  } while (0)

static Track makeTrack(const std::string &title) {
  Track t;
  t.path = fs::path(title + ".mp3");
  t.title = title;
  return t;
}

class FakePlaybackEngine : public PlaybackEngine {
public:
  bool loaded = false;
  bool running = false;
  bool startAllowed = true;

  bool load(const std::string &) override {
    loaded = true;
    running = false;
    return true;
  }
  void unload() override { loaded = running = false; }
  bool play() override {
    running = loaded && startAllowed;
    return running;
  }
  void pause() override { running = false; }
  void gotoSecond(float) override {}
  float getDuration() override { return 0; }
  bool isFinish() override { return false; }
  void setVolume(float) override {}
  float getCurrentPosition() override { return 0; }
  void getRecentSamples(float *out, size_t n) const override {
    std::fill_n(out, n, 0.f);
  }
  int sampleRate() const override { return 44100; }
};

static bool testPlayback() {
  FakePlaybackEngine engine;
  Player player(engine);
  player.toggle();
  CHECK(!player.isPlaying() && !engine.running);

  player.enqueue(makeTrack("A"));
  player.toggle();
  CHECK(player.isPlaying() && engine.running);
  player.toggle();
  CHECK(!player.isPlaying() && !engine.running && engine.loaded);
  player.toggle();
  CHECK(player.isPlaying() && engine.running);

  player.removeQueueIndex(0);
  CHECK(!player.currentTrack() && !player.isPlaying());
  CHECK(!engine.loaded && !engine.running);
  player.toggle();
  CHECK(!player.isPlaying() && !engine.running);

  player.enqueue(makeTrack("B"));
  engine.startAllowed = false;
  player.playQueueIndex(0);
  CHECK(!player.isPlaying());
  player.toggle();
  CHECK(!player.isPlaying());
  player.clear();
  CHECK(!engine.loaded && !player.currentTrack());
  return true;
}

static bool testQueue() {
  Queue q;
  CHECK(!q.current());
  CHECK(!q.next());

  q.add(makeTrack("A"));
  q.add(makeTrack("B"));
  q.add(makeTrack("C"));
  CHECK(q.current() && q.current()->title == "A");
  CHECK(q.currentIndex() == 0);

  CHECK(q.next() && q.current()->title == "B");
  CHECK(q.previous() && q.current()->title == "A");
  CHECK(q.jumpTo(2) && q.current()->title == "C");
  CHECK(!q.next());

  q.setRepeat(RepeatMode::All);
  CHECK(q.next() && q.current()->title == "A");
  q.setRepeat(RepeatMode::One);
  CHECK(q.next(true) && q.current()->title == "A");
  q.setRepeat(RepeatMode::Off);

  CHECK(q.remove(1));
  CHECK(q.tracks().size() == 2);
  CHECK(q.current()->title == "A");
  CHECK(q.next() && q.current()->title == "C");

  q.jumpTo(0);
  CHECK(q.move(1, -1));
  CHECK(q.tracks()[0].title == "C");
  CHECK(q.tracks()[1].title == "A");
  CHECK(q.current()->title == "A");

  q.clear();
  CHECK(q.tracks().empty());
  CHECK(!q.current());
  return true;
}

static std::string readBytes(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f),
                     std::istreambuf_iterator<char>());
}

static bool testPlaylists() {
  char tmpl[] = "/tmp/musica-tests-XXXXXX";
  const char *dir = mkdtemp(tmpl);
  CHECK(dir != nullptr);
  setenv("XDG_CONFIG_HOME", dir, 1);
  const fs::path cfg(dir);
  const fs::path plDir = cfg / "musica" / "playlists";

  PlaylistStore store;
  CHECK(store.list().empty());
  CHECK(store.create("Road"));
  CHECK(!store.create("Road"));

  std::vector<fs::path> loaded;
  // A valid empty playlist is readable, not an error.
  CHECK(store.load("Road", loaded));
  CHECK(loaded.empty());

  const std::vector<fs::path> tracks = {"/music/a.mp3", "/music/b.mp3"};
  CHECK(store.save("Road", tracks));
  CHECK(store.load("Road", loaded));
  CHECK(loaded == tracks);
  CHECK(store.addTrack("Road", "/music/c.mp3"));
  CHECK(store.load("Road", loaded));
  CHECK(loaded.size() == 3);
  CHECK(loaded[2].is_absolute());

  // An unreadable playlist must make addTrack fail instead of rewriting it as
  // a one-track list. A directory at the .m3u path fails even as root.
  std::error_code ec;
  CHECK(fs::create_directory(plDir / "Broken.m3u", ec) && !ec);
  CHECK(!store.addTrack("Broken", "/music/d.mp3"));
  CHECK(fs::is_directory(plDir / "Broken.m3u"));

  // CRLF, comments, relative paths with spaces and a duplicate, read while the
  // working directory is somewhere else.
  const fs::path cwdBefore = fs::current_path();
  fs::create_directories(cfg / "elsewhere");
  fs::current_path(cfg / "elsewhere");
  {
    std::ofstream f(plDir / "Portable.m3u", std::ios::binary | std::ios::trunc);
    f << "#EXTM3U\r\n"
         "#EXTINF:12,Artist - Title\r\n"
         "\r\n"
         "album one/a b.mp3\r\n"
         "/srv/absolute/x.mp3\r\n"
         "album one/a b.mp3\r\n";
  }
  CHECK(store.load("Portable", loaded));
  CHECK(loaded.size() == 3);
  CHECK(loaded[0] == plDir / "album one/a b.mp3");
  CHECK(loaded[1] == fs::path("/srv/absolute/x.mp3"));
  CHECK(loaded[2] == plDir / "album one/a b.mp3");
  fs::current_path(cwdBefore);

  // A failed save keeps the original byte for byte and leaves no temp file.
  CHECK(store.save("Safe", {"/music/safe.mp3"}));
  const fs::path safeFile = plDir / "Safe.m3u";
  const std::string before = readBytes(safeFile);
  struct rlimit oldLimit {};
  CHECK(getrlimit(RLIMIT_FSIZE, &oldLimit) == 0);
  struct rlimit smallLimit = oldLimit;
  smallLimit.rlim_cur = 4;
  const auto oldHandler = std::signal(SIGXFSZ, SIG_IGN);
  CHECK(setrlimit(RLIMIT_FSIZE, &smallLimit) == 0);
  const bool failed =
      store.save("Safe", {"/music/one.mp3", "/music/two.mp3"});
  CHECK(setrlimit(RLIMIT_FSIZE, &oldLimit) == 0);
  std::signal(SIGXFSZ, oldHandler);
  CHECK(!failed);
  CHECK(readBytes(safeFile) == before);
  for (const auto &entry : fs::directory_iterator(plDir))
    CHECK(entry.path().filename().string().find(".tmp") == std::string::npos);

  CHECK(store.rename("Road", "Trip"));
  CHECK((store.list() == std::vector<std::string>{"Broken", "Portable", "Safe",
                                                    "Trip"}));
  CHECK(store.remove("Trip"));

  fs::remove_all(dir, ec);
  unsetenv("XDG_CONFIG_HOME");
  return true;
}

static bool testSpectrum() {
  Spectrum spectrum;
  std::vector<float> samples(Spectrum::kFft);
  for (size_t i = 0; i < samples.size(); ++i)
    samples[i] =
        0.02f * std::sin(2.0f * 3.14159265f * 220.0f * i / 44100.0f);

  spectrum.update(samples.data(), 44100);
  size_t peak = 0;
  for (size_t i = 1; i < spectrum.bands().size(); ++i)
    if (spectrum.bands()[i] > spectrum.bands()[peak])
      peak = i;
  CHECK(peak == 9);
  CHECK(spectrum.bands()[9] > 0.3f);

  for (int i = 0; i < 100; ++i)
    spectrum.silence();
  for (float v : spectrum.bands())
    CHECK(v == 0.0f);
  return true;
}

int main() {
  const bool ok =
      testPlayback() && testQueue() && testPlaylists() && testSpectrum();
  return ok ? 0 : 1;
}
