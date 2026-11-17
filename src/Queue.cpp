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
