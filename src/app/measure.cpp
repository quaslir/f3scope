#include "app/measure.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr uint8_t MIN_SPAN = 50;
}
namespace app::measure {
Measurements measure(const uint16_t *data, uint16_t n,
                     uint32_t sample_rate_hz) {
  if (n == 0)
    return Measurements{};
  Measurements meas{};
  meas.min = data[0];
  meas.max = data[0];
  uint64_t sum = 0;
  uint64_t sum_sq = 0;
  for (uint16_t i = 0; i < n; i++) {
    meas.min = std::min(data[i], meas.min);
    meas.max = std::max(data[i], meas.max);
    sum += data[i];
    sum_sq += data[i] * data[i];
  }

  meas.mean = sum / n;
  meas.rms = std::lround(std::sqrt(static_cast<double>(sum_sq) / n));
  int32_t mid = (meas.min + meas.max) / 2;
  uint16_t diff = (meas.max - meas.min);
  int32_t hyst = diff / 10;
  if (diff < MIN_SPAN) {
    return meas;
  }

  bool armed = false;
  uint16_t first = 0, last = 0;
  uint16_t count = 0;

  for(uint16_t i =0; i < n; i++) {
      if(!armed && data[i] <= mid - hyst) armed = true;
      else if(armed && data[i] >= mid) {
          if(count == 0) first = i;
          else last = i;
          count++;
          armed = false;
      }
  }

  if(count < 2) {
      return meas;
  }

  meas.periodic = true;
  uint64_t freq = static_cast<uint64_t>(sample_rate_hz) * 1000 * (count - 1) / (last - first);
  meas.freq_mhz = static_cast<uint32_t>(freq);
uint32_t high = 0;
for(uint16_t i = first; i < last; i++) {
    if(data[i] >= mid ) {
       high++;
    }
}

meas.duty_permille = high * 1000 / (last - first);


  return meas;
}
} // namespace app::measure
