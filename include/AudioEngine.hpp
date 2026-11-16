#pragma once

#include "miniaudio.h"
#include <string>

struct TrackMetadata {
  std::string title = "Title unknown";
  std::string artist = "Artist unknown";
  std::string album = "Album unknown";
  unsigned int trackNumber;
  int bitrate = 0;
};

class AudioEngine {
private:
  ma_engine engine;
  ma_sound sound;
  bool engineInitialized = false;
  bool isLoaded = false;
  float volume_ = 1.0f;
  void unload();

public:
  AudioEngine();
  ~AudioEngine();
  AudioEngine(const AudioEngine &) = delete;
  AudioEngine &operator=(const AudioEngine &) = delete;

  bool load(const std::string &filename);
  void play();
  void pause();
  void gotoSecond(float seconds);
  float getDuration();
  bool isFinish();
  void setVolume(float volume);
  bool isInitialized();
  void setLoop(bool enable);
  float getCurrentPosition();
};
