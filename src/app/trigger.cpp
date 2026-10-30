#include "app/trigger.hpp"
namespace app::trigger {
std::optional<uint16_t> find(const uint16_t *data, uint16_t n, uint16_t level,
                             uint16_t hysteresis, Edge edge) {
  bool armed = false;

  for (uint16_t i = 0; i < n; i++) {

    if (edge == Edge::Rising) {

      if (!armed && data[i] <= level - hysteresis) {
        armed = true;
      } else if (armed && data[i] >= level)
        return i;
    } else {
      if (!armed && data[i] >= level + hysteresis) {
        armed = true;
      } else if (armed && data[i] <= level)
        armed = true;
    }
  }

  return std::nullopt;
}
} // namespace app::trigger
