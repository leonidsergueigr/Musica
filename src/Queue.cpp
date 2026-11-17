#include "Queue.hpp"
#include <algorithm>
#include <numeric>
#include <random>

void Queue::add(Track t) {
  tracks_.push_back(std::move(t));
  order_.push_back(tracks_.size() - 1);
}

void Queue::replace(std::vector<Track> tracks, size_t start) {
  tracks_ = std::move(tracks);
  order_.resize(tracks_.size());
  std::iota(order_.begin(), order_.end(), 0);
  pos_ = std::min(start, tracks_.empty() ? 0 : tracks_.size() - 1);
  if (shuffle_)
    rebuildOrder();
}

void Queue::clear() {
  tracks_.clear();
  order_.clear();
  pos_ = 0;
}

const Track *Queue::current() const {
  return pos_ < order_.size() ? &tracks_[order_[pos_]] : nullptr;
}

size_t Queue::currentIndex() const {
  return pos_ < order_.size() ? order_[pos_] : 0;
}

bool Queue::next(bool autoAdvance) {
  if (order_.empty())
    return false;
  if (autoAdvance && repeat_ == RepeatMode::One)
    return true;
  if (pos_ + 1 < order_.size()) {
    ++pos_;
    return true;
  }
  if (repeat_ == RepeatMode::All) {
    pos_ = 0;
    return true;
  }
  return false;
}

bool Queue::previous() {
  if (order_.empty())
    return false;
  if (pos_ > 0)
    --pos_;
  return true;
}

bool Queue::jumpTo(size_t idx) {
  auto it = std::find(order_.begin(), order_.end(), idx);
  if (it == order_.end())
    return false;
  pos_ = it - order_.begin();
  return true;
}

bool Queue::remove(size_t trackIndex) {
  const auto it = std::find(order_.begin(), order_.end(), trackIndex);
  if (it == order_.end())
    return false;
  const size_t p = static_cast<size_t>(it - order_.begin());

  order_.erase(order_.begin() + static_cast<long>(p));
  for (auto &o : order_)
    if (o > trackIndex)
      --o;
  tracks_.erase(tracks_.begin() + static_cast<long>(trackIndex));

  if (order_.empty()) {
    pos_ = 0;
    return true;
  }
  if (p < pos_)
    --pos_;
  else if (p == pos_)
    pos_ = std::min(pos_, order_.size() - 1);
  return true;
}

bool Queue::move(size_t trackIndex, int delta) {
  const long to = static_cast<long>(trackIndex) + delta;
  if (to < 0 || to >= static_cast<long>(tracks_.size()))
    return false;
  const size_t target = static_cast<size_t>(to);
  if (target == trackIndex)
    return true;

  // Les identifiants de order_ désignent des emplacements : on échange le
  // contenu de deux emplacements et on suit la piste en cours de lecture.
  const size_t cur = order_.empty() ? 0 : order_[pos_];
  std::swap(tracks_[trackIndex], tracks_[target]);
  size_t curId = cur;
  if (cur == trackIndex)
    curId = target;
  else if (cur == target)
    curId = trackIndex;
  const auto it = std::find(order_.begin(), order_.end(), curId);
  if (it != order_.end())
    pos_ = static_cast<size_t>(it - order_.begin());
  return true;
}

void Queue::setShuffle(bool on) {
  shuffle_ = on;
  rebuildOrder();
}

void Queue::rebuildOrder() {
  if (order_.empty())
    return;
  size_t cur = order_[pos_];
  std::iota(order_.begin(), order_.end(), 0);
  if (shuffle_) {
    static std::mt19937 rng{std::random_device{}()};
    std::shuffle(order_.begin(), order_.end(), rng);
    auto it = std::find(order_.begin(), order_.end(), cur);
    std::iter_swap(order_.begin(), it);
    pos_ = 0;
  } else {
    pos_ = cur;
  }
}
