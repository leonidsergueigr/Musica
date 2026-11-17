#pragma once
#include "Track.hpp"

#include <vector>

enum class RepeatMode { Off, All, One };

class Queue {
public:
  void add(Track t);
  void replace(std::vector<Track> tracks, size_t startIndex = 0);
  void clear();

  const Track *current() const;
  const std::vector<Track> &tracks() const { return tracks_; }
  size_t currentIndex() const;
  bool next(bool autoAdvance = false);
  bool previous();
  bool jumpTo(size_t trackIndex);
  // Indices refer to tracks_. Both operations keep the playing track and the
  // shuffle order consistent.
  bool remove(size_t trackIndex);
  bool move(size_t trackIndex, int delta);

  void setShuffle(bool on);
  bool shuffle() const { return shuffle_; }
  void setRepeat(RepeatMode m) { repeat_ = m; }
  RepeatMode repeat() const { return repeat_; }

private:
  void rebuildOrder();
  std::vector<Track> tracks_;
  std::vector<size_t> order_;
  size_t pos_ = 0;
  bool shuffle_ = false;
  RepeatMode repeat_ = RepeatMode::Off;
};
