
#include "drivers/uart.hpp"
#include <cstdio>

#include "bsp/bsp.hpp"
#include "drivers/adc.hpp"
int main() {
        bsp::init();
        drivers::uart::init(115200);
        drivers::adc::init();
        bsp::set_led(true);

        std::printf("\nf3scope ADC test, SYSCLK = %lu Hz\n", bsp::sysclk_hz());

            while (true) {
                const uint16_t raw = drivers::adc::read();
                const uint32_t mv  = static_cast<uint32_t>(raw) * 3300u / 4095u;

                std::printf("raw=%4u  mv=%4lu\n", raw, mv);

                bsp::toggle_led();
                bsp::delay_ms(200);
            }
}
