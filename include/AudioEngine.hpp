#pragma once

#include "miniaudio.h"

#include <atomic>
#include <cstddef>
#include <string>

struct TapNode {
  static constexpr size_t kSize = 4096;
  ma_node_base base;
  ma_uint32 channels = 2;
  float buf[kSize] = {};
  std::atomic<size_t> writePos{0};
};

class AudioEngine {
private:
  ma_engine engine_;
  ma_sound sound_;
  TapNode tap_;
  bool engineInitialized_ = false;
  bool tapInitialized_ = false;
  bool isLoaded_ = false;
  float volume_ = 1.0f;
  void unload();
  void initTap();

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
  float getVolume() const { return volume_; }
  bool isInitialized() const { return engineInitialized_; }
  float getCurrentPosition();

  void getRecentSamples(float *out, size_t n) const;
  int sampleRate() const;
};
