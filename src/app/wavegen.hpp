#pragma once
#include <cstdint>
namespace app::wavegen {
    void fill_sine(uint16_t * table, uint16_t n, uint16_t offset, uint16_t amplitude);
}
