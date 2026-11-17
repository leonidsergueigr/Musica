#include "Player.hpp"
#include <algorithm>

void Player::loadCurrent(bool start) {
  const Track *t = queue_.current();
  if (!t || !engine_.load(t->path.string())) {
    playing_ = false;
    return;
  }
  engine_.setVolume(volume_);
  if (start) {
    engine_.play();
    playing_ = true;
  }
}

void Player::playQueueIndex(size_t i) {
  if (queue_.jumpTo(i))
    loadCurrent(true);
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
  float target = std::clamp(position() + d, 0.0f, duration());
  engine_.gotoSecond(target);
}

void Player::setVolume(float v) {
  volume_ = std::clamp(v, 0.0f, 1.0f);
  engine_.setVolume(volume_);
}

void Player::tick() {
  if (playing_ && engine_.isFinish()) {
    if (queue_.next(true))
      loadCurrent(true);
    else
      playing_ = false;
  }
}
