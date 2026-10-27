#include "stm32f303xe.h"
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"

#include "bsp/bsp.hpp"
int main() {
        bsp::init();
        bsp::set_led(true);


        while(1) {
                bsp::toggle_led();
                HAL_Delay(500);
        }
}
