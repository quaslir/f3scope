#pragma once
#include <cstdint>
namespace drivers::dac {
    void init();
    void set(uint16_t value);
    void start_wave(const uint16_t* table, uint16_t n);
}
