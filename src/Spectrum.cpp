#include "Spectrum.hpp"
#include <algorithm>
#include <cmath>
#include <complex>

namespace {

constexpr float kPi = 3.14159265358979323846f;

void fft(std::vector<std::complex<float>> &a) {
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1) {
    float ang = -2.0f * kPi / static_cast<float>(len);
    std::complex<float> wlen(std::cos(ang), std::sin(ang));
    for (size_t i = 0; i < n; i += len) {
      std::complex<float> w(1.0f, 0.0f);
      for (size_t j = 0; j < len / 2; ++j) {
        auto u = a[i + j];
        auto v = a[i + j + len / 2] * w;
        a[i + j] = u + v;
        a[i + j + len / 2] = u - v;
        w *= wlen;
      }
    }
  }
}

} // namespace

Spectrum::Spectrum() : window_(kFft) {
  for (size_t i = 0; i < kFft; ++i)
    window_[i] = 0.5f * (1.0f - std::cos(2.0f * kPi * i / (kFft - 1)));
}

void Spectrum::update(const float *samples, int sampleRate) {
  std::vector<std::complex<float>> data(kFft);
  for (size_t i = 0; i < kFft; ++i)
    data[i] = {samples[i] * window_[i], 0.0f};
  fft(data);

  const float binHz = static_cast<float>(sampleRate) / kFft;
  const float fMin = 40.0f;
  const float fMax = std::min(16000.0f, sampleRate / 2.0f);
  const float ratio = fMax / fMin;

  std::array<float, kBands> target{};
  for (size_t b = 0; b < kBands; ++b) {
    float f0 = fMin * std::pow(ratio, static_cast<float>(b) / kBands);
    float f1 = fMin * std::pow(ratio, static_cast<float>(b + 1) / kBands);
    size_t k0 = static_cast<size_t>(f0 / binHz);
    size_t k1 = std::max(k0 + 1, static_cast<size_t>(std::ceil(f1 / binHz)));
    k0 = std::max<size_t>(k0, 1);
    k1 = std::min(k1, kFft / 2);

    float peak = 0.0f;
    for (size_t k = k0; k < k1; ++k)
      peak = std::max(peak, std::abs(data[k]));

    float amp = peak * 4.0f / kFft;
    float db = 20.0f * std::log10(amp + 1e-6f);

    db += 14.0f * static_cast<float>(b) / kBands;

    target[b] = std::clamp((db + 60.0f) / 48.0f, 0.0f, 1.0f);
  }
  smooth(target);
}

void Spectrum::silence() {
  std::array<float, kBands> zero{};
  smooth(zero);
}

void Spectrum::smooth(const std::array<float, kBands> &target) {
  for (size_t i = 0; i < kBands; ++i) {
    float &v = bands_[i];
    if (target[i] > v)
      v += (target[i] - v) * 0.7f;
    else
      v = std::max(target[i], v - 0.06f);
  }
}
