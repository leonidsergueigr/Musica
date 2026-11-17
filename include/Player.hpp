#pragma once
#include "AudioEngine.hpp"
#include "Queue.hpp"
#include "Spectrum.hpp"

class Player {
public:
  bool init() { return engine_.isInitialized(); }

  Queue &queue() { return queue_; }
  void playQueueIndex(size_t i);
  void removeQueueIndex(size_t i);
  void moveQueueIndex(size_t i, int delta);
  void clear();
  void enqueue(const Track &t);

  void play();
  void pause();
  void toggle();
  void next();
  void previous();
  void seekRelative(float deltaSec);
  void seekTo(float sec);
  void setVolume(float v);
  float volume() const { return volume_; }

  bool isPlaying() const { return playing_; }
  float position() { return engine_.getCurrentPosition(); }
  float duration() { return engine_.getDuration(); }
  const Track *currentTrack() const { return queue_.current(); }
  const Spectrum &spectrum() const { return spectrum_; }
  void tick();

  // Reports that playback failed because every candidate track was skipped.
  // Returns the number of skipped tracks through `skipped` and clears the flag.
  bool takeLoadError(int &skipped);

private:
  void loadCurrent(bool startPlaying);
  AudioEngine engine_;
  Queue queue_;
  float volume_ = 1.0f;
  bool playing_ = false;
  bool loadError_ = false;
  int skippedCount_ = 0;
  void updateSpectrum();
  Spectrum spectrum_;
};
