#pragma once

namespace bsp {
    void init();
   [[noreturn]] void error_handler();
    void set_led(bool on);
    void toggle_led();
}
