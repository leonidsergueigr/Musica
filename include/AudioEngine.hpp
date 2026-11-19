#pragma once

#include "PlaybackEngine.hpp"
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

class AudioEngine : public PlaybackEngine {
private:
  ma_engine engine_;
  ma_sound sound_;
  TapNode tap_;
  bool engineInitialized_ = false;
  bool tapInitialized_ = false;
  bool isLoaded_ = false;
  float volume_ = 1.0f;
  void initTap();

public:
  AudioEngine();
  ~AudioEngine() override;
  AudioEngine(const AudioEngine &) = delete;
  AudioEngine &operator=(const AudioEngine &) = delete;

  bool load(const std::string &filename) override;
  void unload() override;
  bool play() override;
  void pause() override;
  void gotoSecond(float seconds) override;
  float getDuration() override;
  bool isFinish() override;
  void setVolume(float volume) override;
  float getVolume() const { return volume_; }
  bool isInitialized() const { return engineInitialized_; }
  float getCurrentPosition() override;

  void getRecentSamples(float *out, size_t n) const override;
  int sampleRate() const override;
};
