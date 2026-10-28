#pragma once
#include <cstdint>
namespace drivers::adc {
void init();
[[nodiscard]] uint16_t read();
void start_capture(uint16_t * buffer, uint16_t count);
[[nodiscard]] bool capture_done();
}
