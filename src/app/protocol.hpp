#pragma once
#include <cstdint>
namespace app::protocol {

inline constexpr uint8_t SYNC0 = 0xAA;
inline constexpr uint8_t SYNC1 = 0x55;
inline constexpr uint8_t HEADER_SIZE = 4;
inline constexpr uint8_t FOOTER_SIZE = 2;
uint16_t checksum(const uint16_t *samples, uint16_t count);
void write_header(uint8_t *out, uint16_t count);
void write_footer(uint8_t *out, uint16_t count);
} // namespace app::protocol
