#include "drivers/uart.hpp"
#include "drivers/adc.hpp"
#include "drivers/tim.hpp"
#include "drivers/dac.hpp"
#include "app/wavegen.hpp"
#include "bsp/bsp.hpp"
#include "bsp/board.hpp"
#include <cstdint>
#include <cstdio>

constexpr uint16_t FRAME_SIZE = 1000;
constexpr uint16_t SINE_POINTS = 100;

uint16_t frame[FRAME_SIZE];
uint16_t sine_table[SINE_POINTS];

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
    dac_timer.init(100'000);

    // --- signal generator: 100 kHz / 100 points = 1 kHz sine on A2 ---
    app::wavegen::fill_sine(sine_table, SINE_POINTS, 2048, 1700);
    drivers::dac::start_wave(sine_table, SINE_POINTS);
    dac_timer.start();
    bsp::delay_ms(10);

    // --- capture one frame on A0 ---
    drivers::adc::start_capture(frame, FRAME_SIZE);
    adc_timer.start();
    while (!drivers::adc::capture_done()) {
    }
    adc_timer.stop();

    // --- results ---
    uint16_t min = 4095, max = 0;
    for (uint16_t i = 0; i < FRAME_SIZE; ++i) {
        if (frame[i] < min) min = frame[i];
        if (frame[i] > max) max = frame[i];
    }
    std::printf("\nsine 1 kHz: min=%u max=%u\n", min, max);

    std::printf("one period (100 samples):\n");
    for (uint16_t i = 0; i < SINE_POINTS; ++i) {
        std::printf("%4u ", frame[i]);
        if (i % 10 == 9) std::printf("\n");
    }

    std::printf("period check: frame[0]=%u frame[100]=%u frame[200]=%u\n",
                frame[0], frame[100], frame[200]);

    while (true) {
        bsp::toggle_led();
        bsp::delay_ms(500);
    }
}
