#include "AudioEngine.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

// Runs on the real-time audio thread: no allocation, no locking, no blocking
// call. It must never write past *pFrameCountOut frames and must always fill
// the output (silence included), otherwise uninitialized samples are played.
static void tapProcess(ma_node *pNode, const float **ppFramesIn,
                       ma_uint32 *pFrameCountIn, float **ppFramesOut,
                       ma_uint32 *pFrameCountOut) {
  auto *tap = reinterpret_cast<TapNode *>(pNode);
  const ma_uint32 frames = std::min(*pFrameCountIn, *pFrameCountOut);
  const float *in = ppFramesIn[0];
  float *out = ppFramesOut[0];
  const ma_uint32 ch = tap->channels;
  if (!in || !out || frames == 0)
    return;

  size_t w = tap->writePos.load(std::memory_order_relaxed);

  if (ch == 2) { // stereo is the common case: keep the per-frame loop simple
    for (ma_uint32 i = 0; i < frames; ++i) {
      const float l = in[2 * i];
      const float r = in[2 * i + 1];
      out[2 * i] = l;
      out[2 * i + 1] = r;
      tap->buf[w & (TapNode::kSize - 1)] = (l + r) * 0.5f;
      ++w;
    }
  } else {
    const float inv = 1.0f / static_cast<float>(ch);
    for (ma_uint32 i = 0; i < frames; ++i) {
      float sum = 0.0f;
      for (ma_uint32 c = 0; c < ch; ++c) {
        const float s = in[i * ch + c];
        out[i * ch + c] = s;
        sum += s;
      }
      tap->buf[w & (TapNode::kSize - 1)] = sum * inv;
      ++w;
    }
  }
  tap->writePos.store(w, std::memory_order_release);

  // The output buffer can be larger than the input (rare); pad with silence
  // instead of leaving indeterminate samples.
  if (*pFrameCountOut > frames) {
    std::memset(out + static_cast<size_t>(frames) * ch, 0,
                static_cast<size_t>(*pFrameCountOut - frames) * ch *
                    sizeof(float));
  }

  *pFrameCountIn = frames;
  *pFrameCountOut = frames;
}

static ma_node_vtable tapVtable = {tapProcess, nullptr, 1, 1, 0};

AudioEngine::AudioEngine() {
  if (ma_engine_init(NULL, &engine_) == MA_SUCCESS) {
    engineInitialized_ = true;
    initTap();
  } else {
    std::cerr << "Failed to initialize the audio engine" << std::endl;
  }
}

AudioEngine::~AudioEngine() {
  unload();
  if (tapInitialized_)
    ma_node_uninit(&tap_.base, NULL);
  if (engineInitialized_)
    ma_engine_uninit(&engine_);
}

void AudioEngine::initTap() {
  tap_.channels = ma_engine_get_channels(&engine_);
  ma_uint32 ch = tap_.channels;

  ma_node_config cfg = ma_node_config_init();
  cfg.vtable = &tapVtable;
  cfg.pInputChannels = &ch;
  cfg.pOutputChannels = &ch;

  if (ma_node_init(&engine_.nodeGraph, &cfg, NULL, &tap_.base) == MA_SUCCESS) {
    ma_node_attach_output_bus(&tap_.base, 0, ma_engine_get_endpoint(&engine_),
                              0);
    tapInitialized_ = true;
  } else {
    std::cerr << "Spectrum tap unavailable" << std::endl;
  }
}

void AudioEngine::unload() {
  if (!isLoaded_)
    return;
  ma_sound_uninit(&sound_);
  isLoaded_ = false;
}

bool AudioEngine::load(const std::string &filename) {
  if (!engineInitialized_)
    return false;
  unload();

  ma_uint32 flags = MA_SOUND_FLAG_STREAM;
  if (tapInitialized_)
    flags |= MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT;

  if (ma_sound_init_from_file(&engine_, filename.c_str(), flags, NULL, NULL,
                              &sound_) == MA_SUCCESS) {
    isLoaded_ = true;
    if (tapInitialized_)
      ma_node_attach_output_bus(&sound_, 0, &tap_.base, 0);
    ma_sound_set_volume(&sound_, volume_);
  } else {
    std::cerr << "Failed to load file: " << filename << std::endl;
  }
  return isLoaded_;
}

void AudioEngine::play() {
  if (isLoaded_)
    ma_sound_start(&sound_);
}

void AudioEngine::pause() {
  if (isLoaded_)
    ma_sound_stop(&sound_);
}

void AudioEngine::gotoSecond(float seconds) {
  if (isLoaded_)
    ma_sound_seek_to_second(&sound_, seconds);
}

float AudioEngine::getDuration() {
  float duration = 0.0f;
  if (isLoaded_)
    ma_sound_get_length_in_seconds(&sound_, &duration);
  return duration;
}

bool AudioEngine::isFinish() {
  if (!isLoaded_)
    return false;
  return ma_sound_at_end(&sound_) == MA_TRUE;
}

void AudioEngine::setVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
  if (isLoaded_)
    ma_sound_set_volume(&sound_, volume_);
}

float AudioEngine::getCurrentPosition() {
  float cursor = 0.0f;
  if (isLoaded_)
    ma_sound_get_cursor_in_seconds(&sound_, &cursor);
  return cursor;
}

void AudioEngine::getRecentSamples(float *out, size_t n) const {
  n = std::min(n, TapNode::kSize);
  size_t w = tap_.writePos.load(std::memory_order_acquire);
  for (size_t i = 0; i < n; ++i)
    out[i] = tap_.buf[(w - n + i) & (TapNode::kSize - 1)];
}

int AudioEngine::sampleRate() const {
  return static_cast<int>(ma_engine_get_sample_rate(&engine_));
}
