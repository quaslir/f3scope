#pragma once
#include <cstdint>
namespace app::measure {
struct Measurements {
  uint16_t min{}, max{};
  uint16_t mean{};
  uint16_t rms{};
  bool periodic{};
  uint32_t freq_mhz{};
  uint16_t duty_permille{};
};

[[nodiscard]] Measurements measure(const uint16_t *data, uint16_t n,
                                   uint32_t sample_rate_hz);
} // namespace app::measure
