#include "app/wavegen.hpp"
#include <cmath>
#include <algorithm>

namespace app::wavegen {
    constexpr float PI = 3.14159265f;
    void fill_sine(uint16_t * table, uint16_t n, uint16_t offset, uint16_t amplitude) {
        for(uint16_t i = 0; i < n; i++) {
            float result =  offset + amplitude * std::sin(2 * PI * i / n);
            table[i] = static_cast<uint16_t>(std::clamp<long>(std::lround(result), 0, 4095));
        }
    }
}
