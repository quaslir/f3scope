#pragma once

#include <cstdint>

namespace bsp {
    void init();
   [[noreturn]] void error_handler();
    void set_led(bool on);
    void toggle_led();
    uint32_t sysclk_hz();
    void delay_ms(uint32_t ms);
}
