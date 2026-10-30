#pragma once
#include <cstdint>
#include <optional>
namespace app::trigger {
enum class Edge : uint8_t {
  Rising,
  Falling,
};
std::optional<uint16_t> find(const uint16_t *data, uint16_t n, uint16_t level,
                             uint16_t hysteresis, Edge edge);
} // namespace app::trigger
