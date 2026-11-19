#pragma once

#include <cstddef>
#include <string>

class PlaybackEngine {
public:
  virtual ~PlaybackEngine() = default;
  virtual bool load(const std::string &filename) = 0;
  virtual void unload() = 0;
  virtual bool play() = 0;
  virtual void pause() = 0;
  virtual void gotoSecond(float seconds) = 0;
  virtual float getDuration() = 0;
  virtual bool isFinish() = 0;
  virtual void setVolume(float volume) = 0;
  virtual float getCurrentPosition() = 0;
  virtual void getRecentSamples(float *out, size_t n) const = 0;
  virtual int sampleRate() const = 0;
};
