#include "app/timer_math.hpp"

namespace app::timer_math {

std::optional<Result> calculate(uint32_t rate_hz, uint32_t target_rate_hz) {
  if (target_rate_hz == 0) {
    return std::nullopt;
  }
  auto ticks = rate_hz / target_rate_hz;
  if (ticks == 0) {
    return std::nullopt;
  }
  auto psc = (ticks - 1) / 65536;
  auto arr = ticks / (psc + 1) - 1;
  auto sample_rate = rate_hz / ((psc + 1) * (arr + 1));
  return Result{.arr = arr, .psc = psc, .sample_rate = sample_rate};
}

} // namespace app::timer_math
