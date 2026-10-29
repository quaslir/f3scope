#include "drivers/uart.hpp"
#include "drivers/adc.hpp"
#include "drivers/tim.hpp"
#include "drivers/dac.hpp"
#include "bsp/bsp.hpp"
#include "bsp/board.hpp"
#include <cstdint>
#include <cstdio>

uint16_t frame[1000];
drivers::BasicTimer adc_timer{bsp::board::ADC_TIMER};
drivers::BasicTimer dac_timer{bsp::board::DAC_TIMER};

int main() {
    bsp::init();
    bsp::delay_ms(3000);
    drivers::uart::init(115200);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    drivers::adc::init();
    drivers::dac::init();
    adc_timer.init(100'000);
    drivers::dac::set(2048);          // ~1.65 V on A2, set once
    bsp::delay_ms(10);                // let the output settle
    drivers::adc::start_capture(frame, 1000);
    adc_timer.start();
    while (!drivers::adc::capture_done());
    adc_timer.stop();
    uint16_t min = 4095, max = 0;
    for (uint16_t i = 0; i < 1000; ++i) {
        if (frame[i] < min) min = frame[i];
        if (frame[i] > max) max = frame[i];
    }
    std::printf("DAC=2048 -> min=%u max=%u |", min, max);
    for (uint16_t i = 0; i < 20; ++i) {
        std::printf(" %u", frame[i]);
    }
    std::printf("\n");

    while (true) {
        bsp::toggle_led();
        bsp::delay_ms(500);
    }
}
