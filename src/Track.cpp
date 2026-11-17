#include "Track.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>

bool isAudioFile(const std::filesystem::path &p) {
  std::array<std::string, 6> exts = {".mp3", ".flac", ".wav",
                                     ".ogg", ".m4a",  ".opus"};
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

Track readTrack(const std::filesystem::path &p) {
  Track t;
  t.path = p;
  t.title = p.stem().string();

  TagLib::FileRef f(p.c_str());
  if (f.isNull())
    return t;

  if (auto *tag = f.tag()) {
    if (!tag->title().isEmpty())
      t.title = tag->title().to8Bit(true);
    if (!tag->artist().isEmpty())
      t.artist = tag->artist().to8Bit(true);
    if (!tag->album().isEmpty())
      t.album = tag->album().to8Bit(true);
    t.trackNumber = tag->track();
  }
  if (auto *ap = f.audioProperties()) {
    t.bitrate = ap->bitrate();
    t.durationSec = ap->lengthInSeconds();
  }
  return t;
}
