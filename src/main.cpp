
#include "drivers/uart.hpp"
#include <cstdio>

#include "bsp/bsp.hpp"
#include "drivers/adc.hpp"
#include "drivers/tim.hpp"

uint16_t frame[1000];


int main() {
        bsp::init();
        drivers::uart::init(115200);
        drivers::adc::init();
        drivers::tim::init(100'000);
        drivers::tim::start();
        bsp::set_led(true);
        bsp::delay_ms(3000);
        drivers::adc::start_capture(frame, 1000);
        std::setvbuf(stdout, nullptr, _IONBF, 0);
            while (true) {
                if(drivers::adc::capture_done()) {
                    drivers::tim::stop();

                    std::printf("min/max/first:");
                    uint16_t min = 4095, max = 0;
                    for (uint16_t i = 0; i < 1000; ++i) {
                        if (frame[i] < min) min = frame[i];
                        if (frame[i] > max) max = frame[i];
                    }
                    std::printf(" min=%u max=%u |", min, max);
                    for (uint16_t i = 0; i < 20; ++i) {
                        std::printf(" %u", frame[i]);
                    }
                    std::printf("\n");
                    bsp::error_handler();
                }



            }
}
