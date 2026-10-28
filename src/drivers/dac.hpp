#pragma once
#include <cstdint>
namespace drivers::dac {
    void init();
    void set(uint16_t value);
}
