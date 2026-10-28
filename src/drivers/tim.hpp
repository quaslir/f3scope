#pragma once
#include <cstdint>
namespace drivers::tim {

    void init(uint32_t sample_rate_hz);
    void start();
    void stop();
}
