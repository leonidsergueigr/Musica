#include "PlaylistStore.hpp"
#include "Queue.hpp"
#include "Spectrum.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
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

static bool testPlaylists() {
  char tmpl[] = "/tmp/musica-tests-XXXXXX";
  const char *dir = mkdtemp(tmpl);
  CHECK(dir != nullptr);
  setenv("XDG_CONFIG_HOME", dir, 1);

  PlaylistStore store;
  CHECK(store.list().empty());
  CHECK(store.create("Road"));
  CHECK(!store.create("Road"));

  const std::vector<fs::path> tracks = {"/music/a.mp3", "/music/b.mp3"};
  CHECK(store.save("Road", tracks));
  CHECK(store.load("Road") == tracks);
  CHECK(store.addTrack("Road", "/music/c.mp3"));
  CHECK(store.load("Road").size() == 3);
  CHECK(store.load("Road")[2].is_absolute());

  CHECK(store.rename("Road", "Trip"));
  CHECK(store.list() == std::vector<std::string>{"Trip"});
  CHECK(store.remove("Trip"));
  CHECK(store.list().empty());

  std::error_code ec;
  fs::remove_all(dir, ec);
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
  return testQueue() && testPlaylists() && testSpectrum() ? 0 : 1;
}
