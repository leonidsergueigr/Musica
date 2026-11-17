#include "Player.hpp"
#include <algorithm>

void Player::loadCurrent(bool start) {
  const size_t total = queue_.tracks().size();
  if (total == 0) {
    playing_ = false;
    return;
  }

  // Saute les pistes illisibles, sans jamais boucler indéfiniment (même avec
  // la répétition d'une seule piste).
  size_t skipped = 0;
  for (size_t attempt = 0; attempt < total; ++attempt) {
    const Track *t = queue_.current();
    if (!t)
      break;
    if (engine_.load(t->path.string())) {
      engine_.setVolume(volume_);
      if (start) {
        engine_.play();
        playing_ = true;
      }
      if (skipped > 0) {
        skippedCount_ += static_cast<int>(skipped);
        loadError_ = true;
      }
      return;
    }
    ++skipped;
    if (!queue_.next(true))
      break;
  }

  playing_ = false;
  skippedCount_ += static_cast<int>(skipped);
  loadError_ = true;
}

bool Player::takeLoadError(int &skipped) {
  const bool had = loadError_;
  skipped = skippedCount_;
  loadError_ = false;
  skippedCount_ = 0;
  return had;
}

void Player::playQueueIndex(size_t i) {
  if (queue_.jumpTo(i))
    loadCurrent(true);
}

void Player::clear() {
  engine_.pause();
  queue_.clear();
  playing_ = false;
  loadError_ = false;
  skippedCount_ = 0;
  spectrum_.silence();
}

void Player::enqueue(const Track &t) {
  bool wasEmpty = queue_.tracks().empty();
  queue_.add(t);
  if (wasEmpty)
    loadCurrent(false);
}

void Player::enqueueAll(const std::vector<std::filesystem::path> &paths) {
  for (auto &p : paths)
    enqueue(readTrack(p));
}

void Player::play() {
  engine_.play();
  playing_ = true;
}
void Player::pause() {
  engine_.pause();
  playing_ = false;
}
void Player::toggle() { playing_ ? pause() : play(); }

void Player::next() {
  if (queue_.next())
    loadCurrent(true);
  else {
    engine_.pause();
    playing_ = false;
  }
}

void Player::previous() {
  if (position() > 3.0f) {
    engine_.gotoSecond(0);
    return;
  }
  if (queue_.previous())
    loadCurrent(true);
}

void Player::seekRelative(float d) {
  seekTo(position() + d);
}

void Player::seekTo(float sec) {
  const float target = std::clamp(sec, 0.0f, duration());
  engine_.gotoSecond(target);
}

void Player::setVolume(float v) {
  volume_ = std::clamp(v, 0.0f, 1.0f);
  engine_.setVolume(volume_);
}

void Player::updateSpectrum() {
  if (playing_) {
    std::array<float, Spectrum::kFft> buf;
    engine_.getRecentSamples(buf.data(), buf.size());
    spectrum_.update(buf.data(), engine_.sampleRate());
  } else {
    spectrum_.silence();
  }
}

void Player::tick() {
  updateSpectrum();
  if (playing_ && engine_.isFinish()) {
    if (queue_.next(true))
      loadCurrent(true);
    else {
      engine_.pause();
      playing_ = false;
    }
  }
}
