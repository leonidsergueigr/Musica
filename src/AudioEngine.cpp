#include "AudioEngine.hpp"
#include <iostream>
#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>

AudioEngine::AudioEngine() {
  if (ma_engine_init(NULL, &engine) == MA_SUCCESS) {
    engineInitialized = true;
  } else {
    std::cerr << "Error to initialize engine" << std::endl;
  }
}

AudioEngine::~AudioEngine() {
  unload();
  if (engineInitialized) {
    ma_engine_uninit(&engine);
  }
}

void AudioEngine::unload() {
  if (!isLoaded)
    return;
  ma_sound_uninit(&sound);
  isLoaded = false;
}

bool AudioEngine::load(const std::string &filename) {
  if (isLoaded)
    unload();
  if (ma_sound_init_from_file(&engine, filename.c_str(), MA_SOUND_FLAG_STREAM,
                              NULL, NULL, &sound) == MA_SUCCESS) {
    isLoaded = true;
    ma_sound_set_volume(&sound, volume_);
  } else {
    std::cerr << "Error to load file" << std::endl;
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
  if (isLoaded) {
    ma_sound_get_length_in_seconds(&sound, &duration);
  }
  return duration;
}

bool AudioEngine::isFinish() {
  if (!isLoaded)
    return false;
  return ma_sound_at_end(&sound) == MA_TRUE;
}

void AudioEngine::setVolume(float volume) {
  if (isLoaded)
    ma_sound_set_volume(&sound, volume);
}

bool AudioEngine::isInitialized() { return engineInitialized; }

void AudioEngine::setLoop(bool enable) {
  if (isLoaded)
    ma_sound_set_looping(&sound, enable ? MA_TRUE : MA_FALSE);
}

float AudioEngine::getCurrentPosition() {
  float cursor = 0.0f;
  if (isLoaded) {
    ma_sound_get_cursor_in_seconds(&sound, &cursor);
  }
  return cursor;
}
