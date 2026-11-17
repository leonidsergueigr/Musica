#pragma once
#include <array>
#include <cstddef>
#include <vector>

class Spectrum {
public:
  static constexpr size_t kFft = 1024; // doit être une puissance de 2
  static constexpr size_t kBands = 32;

  Spectrum();

  void update(const float *samples, int sampleRate);
  void silence();

  const std::array<float, kBands> &bands() const { return bands_; }

private:
  void smooth(const std::array<float, kBands> &target);

  std::array<float, kBands> bands_{};
  std::vector<float> window_;
};
