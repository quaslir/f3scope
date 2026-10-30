#include "app/commands.hpp"
#include <charconv>
#include <string_view>
#include <system_error>
namespace {
bool parse_number(std::string_view str, uint32_t &value) {
  uint32_t temp;
  auto [it, ec] = std::from_chars(str.data(), str.data() + str.size(), temp);
  if (ec != std::errc() || it != str.data() + str.size()) {
    return false;
  }
  value = temp;
  return true;
}
} // namespace

namespace app::commands {
Parser::Parser(Settings &settings) : settings_(settings) {}

Result Parser::feed(char c) {
  if (c == '\r')
    return Result::None;
  if (c == '\n') {
    if (overflow_) {
      overflow_ = false;
      count_ = 0;
      return Result::TooLong;
    }

    Result parse_res = parse(std::string_view{buffer_, count_});
    count_ = 0;
    return parse_res;
  }

  if (overflow_)
    return Result::None;
  if (count_ == MAX_LINE_LENGTH) {
    overflow_ = true;
    return Result::None;
  }
  buffer_[count_++] = c;
  return Result::None;
}

Result Parser::parse(std::string_view str) {
  if (str.empty())
    return Result::None;
  auto first_space = str.find(' ');
  std::string_view name{}, value{};
  if (first_space == std::string_view::npos) {
    name = str;
  } else {
    name = str.substr(0, first_space);
    value = str.substr(first_space + 1);
  }
  if (name == "RATE") {
    uint32_t val;
    bool parse_result = parse_number(value, val);
    if (!parse_result)
      return Result::BadValue;
    if (val > RATE_MAX || val < RATE_MIN)
      return Result::BadValue;
    settings_.sample_rate_hz = val;
  } else if (name == "LEVEL") {
    uint32_t val;
    bool parse_result = parse_number(value, val);
    if (!parse_result)
      return Result::BadValue;
    if (val < LEVEL_MIN || val > LEVEL_MAX)
      return Result::BadValue;
    settings_.level = val;
  } else if (name == "EDGE") {
    if (value.size() != 1)
      return Result::BadValue;
    if (value[0] != 'R' && value[0] != 'F')
      return Result::BadValue;

    settings_.edge = static_cast<trigger::Edge>(value[0]);
  } else if (name == "MODE") {
    if (value.size() != 1)
      return Result::BadValue;
    if (value[0] != 'A' && value[0] != 'N' && value[0] != 'S')
      return Result::BadValue;
    if (value[0] == 'S') {
      settings_.single_armed = true;
    }
    settings_.mode = static_cast<Mode>(value[0]);
  } else {
    return Result::UnknownCommand;
  }
  return Result::Ok;
}
} // namespace app::commands
