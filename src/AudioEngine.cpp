#include "AudioEngine.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

// Ce callback tourne dans le thread audio temps-réel : aucune allocation,
// aucun verrou, aucune opération bloquante. Il ne doit rien écrire au-delà de
// *pFrameCountOut trames, et doit toujours remplir la sortie (silence compris)
// sinon le buffer contient des échantillons non initialisés -> grésillements.
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

  if (ch == 2) { // cas courant : boucle déroulée, pas de division par trame
    for (ma_uint32 i = 0; i < frames; ++i) {
      const float l = in[2 * i];
      const float r = in[2 * i + 1];
      out[2 * i] = l; // passe-plat
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

  // Si la sortie est plus grande que l'entrée (rare), on complète avec du
  // silence plutôt que de laisser des échantillons indéterminés.
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
  if (ma_engine_init(NULL, &engine) == MA_SUCCESS) {
    engineInitialized = true;
    initTap();
  } else {
    std::cerr << "Error to initialize engine" << std::endl;
  }
}

AudioEngine::~AudioEngine() {
  unload();
  if (tapInitialized_)
    ma_node_uninit(&tap_.base, NULL);
  if (engineInitialized)
    ma_engine_uninit(&engine);
}

void AudioEngine::initTap() {
  tap_.channels = ma_engine_get_channels(&engine);
  ma_uint32 ch = tap_.channels;

  ma_node_config cfg = ma_node_config_init();
  cfg.vtable = &tapVtable;
  cfg.pInputChannels = &ch;
  cfg.pOutputChannels = &ch;

  if (ma_node_init(&engine.nodeGraph, &cfg, NULL, &tap_.base) == MA_SUCCESS) {
    ma_node_attach_output_bus(&tap_.base, 0, ma_engine_get_endpoint(&engine),
                              0);
    tapInitialized_ = true;
  } else {
    std::cerr << "Spectrum tap unavailable" << std::endl;
  }
}

void AudioEngine::unload() {
  if (!isLoaded)
    return;
  ma_sound_uninit(&sound);
  isLoaded = false;
}

bool AudioEngine::load(const std::string &filename) {
  if (!engineInitialized)
    return false;
  unload();

  ma_uint32 flags = MA_SOUND_FLAG_STREAM;
  if (tapInitialized_)
    flags |= MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT;

  if (ma_sound_init_from_file(&engine, filename.c_str(), flags, NULL, NULL,
                              &sound) == MA_SUCCESS) {
    isLoaded = true;
    if (tapInitialized_)
      ma_node_attach_output_bus(&sound, 0, &tap_.base, 0);
    ma_sound_set_volume(&sound, volume_);
  } else {
    std::cerr << "Error to load file: " << filename << std::endl;
  }
  return isLoaded;
}

void AudioEngine::play() {
  if (isLoaded)
    ma_sound_start(&sound);
}

void AudioEngine::pause() {
  if (isLoaded)
    ma_sound_stop(&sound);
}

void AudioEngine::gotoSecond(float seconds) {
  if (isLoaded)
    ma_sound_seek_to_second(&sound, seconds);
}

float AudioEngine::getDuration() {
  float duration = 0.0f;
  if (isLoaded)
    ma_sound_get_length_in_seconds(&sound, &duration);
  return duration;
}

bool AudioEngine::isFinish() {
  if (!isLoaded)
    return false;
  return ma_sound_at_end(&sound) == MA_TRUE;
}

void AudioEngine::setVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
  if (isLoaded)
    ma_sound_set_volume(&sound, volume_);
}

void AudioEngine::setLoop(bool enable) {
  if (isLoaded)
    ma_sound_set_looping(&sound, enable ? MA_TRUE : MA_FALSE);
}

float AudioEngine::getCurrentPosition() {
  float cursor = 0.0f;
  if (isLoaded)
    ma_sound_get_cursor_in_seconds(&sound, &cursor);
  return cursor;
}

void AudioEngine::getRecentSamples(float *out, size_t n) const {
  n = std::min(n, TapNode::kSize);
  size_t w = tap_.writePos.load(std::memory_order_acquire);
  for (size_t i = 0; i < n; ++i)
    out[i] = tap_.buf[(w - n + i) & (TapNode::kSize - 1)];
}

int AudioEngine::sampleRate() const {
  return static_cast<int>(ma_engine_get_sample_rate(&engine));
}
