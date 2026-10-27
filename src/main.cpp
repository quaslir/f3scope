
#include "stm32f3xx_hal.h"
#include <cstdio>

#include "bsp/bsp.hpp"
int main() {
        bsp::init();
        bsp::set_led(true);


        while(1) {
                bsp::toggle_led();
                bsp::delay_ms(200);
                std::printf("... %lu Hz\n\n", bsp::sysclk_hz());
        }
}
