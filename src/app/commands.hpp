#pragma once
#include "app/trigger.hpp"
#include <cstdint>
#include <string_view>
namespace app::commands {
enum class Mode : char { Auto = 'A', Normal = 'N', Single = 'S' };

inline constexpr uint8_t MAX_LINE_LENGTH = 32;
// Sample rate limits, Hz (TIM6 -> ADC1)
inline constexpr uint32_t RATE_MIN = 1'000;
inline constexpr uint32_t RATE_MAX = 1'000'000;

// Trigger level limits, ADC counts (12-bit)
inline constexpr uint32_t LEVEL_MIN = 0;
inline constexpr uint32_t LEVEL_MAX = (1u << 12) - 1; // 4095

struct Settings {
  uint32_t sample_rate_hz = 100'000;
  uint16_t level = LEVEL_MAX / 2 + 1;
  trigger::Edge edge = trigger::Edge::Rising;
  Mode mode = Mode::Auto;
};
enum class Result : uint8_t { None, Ok, UnknownCommand, BadValue, TooLong };

inline const char *to_string(Result r) {
  switch (r) {
  case Result::None:
    return "none";
  case Result::Ok:
    return "ok";
  case Result::UnknownCommand:
    return "unknown command";
  case Result::BadValue:
    return "bad value";
  case Result::TooLong:
    return "line too long";
  }
  return "?";
}
class Parser {
public:
  explicit Parser(Settings &settings);
  [[nodiscard]] Result feed(char c);

private:
  [[nodiscard]] Result parse(std::string_view str);

  Settings &settings_;
  char buffer_[MAX_LINE_LENGTH];
  uint8_t count_{};
  bool overflow_{};
};
} // namespace app::commands
