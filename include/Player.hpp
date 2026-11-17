#pragma once
#include "AudioEngine.hpp"
#include "Queue.hpp"

class Player {
public:
  bool init() { return engine_.isInitialized(); }

  Queue &queue() { return queue_; }
  void playQueueIndex(size_t i);
  void enqueue(const Track &t);
  void enqueueAll(const std::vector<std::filesystem::path> &paths); // A

  void play();
  void pause();
  void toggle();
  void next();
  void previous();
  void seekRelative(float deltaSec);
  void setVolume(float v);
  float volume() const { return volume_; }

  bool isPlaying() const { return playing_; }
  float position() { return engine_.getCurrentPosition(); }
  float duration() { return engine_.getDuration(); }
  const Track *currentTrack() const { return queue_.current(); }

  void tick();

private:
  void loadCurrent(bool startPlaying);
  AudioEngine engine_;
  Queue queue_;
  float volume_ = 1.0f;
  bool playing_ = false;
};
