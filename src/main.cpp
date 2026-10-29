#include "bsp/bsp.hpp"
#include "bsp/board.hpp"
#include "drivers/uart.hpp"
#include "drivers/tim.hpp"
#include "drivers/adc.hpp"
#include "drivers/dac.hpp"
#include "app/protocol.hpp"
#include "app/wavegen.hpp"
namespace {
    drivers::BasicTimer adc_timer{bsp::board::ADC_TIMER};
    drivers::BasicTimer dac_timer{bsp::board::DAC_TIMER};
    constexpr uint16_t FRAME_SIZE = 1000;
    constexpr uint8_t SINE_POINTS = 100;
    uint16_t frame[FRAME_SIZE];
    uint16_t sine_table[SINE_POINTS];
    uint8_t header[app::protocol::HEADER_SIZE];
    uint8_t footer[app::protocol::FOOTER_SIZE];
}

int main() {
    bsp::init();
    drivers::uart::init(115200);
    drivers::adc::init();
    drivers::dac::init();

    adc_timer.init(100'000);
    dac_timer.init(100'000);

    app::wavegen::fill_sine(sine_table,SINE_POINTS,2048,1700);

    drivers::dac::start_wave(sine_table, SINE_POINTS);
    dac_timer.start();
    while(true) {
        drivers::adc::start_capture(frame, FRAME_SIZE);
        adc_timer.start();
        while(!drivers::adc::capture_done()); // blocking

        adc_timer.stop();
        drivers::adc::stop_capture();
        const uint16_t checksum = app::protocol::checksum(frame, FRAME_SIZE);
        app::protocol::write_header(header, FRAME_SIZE);
        app::protocol::write_footer(footer, checksum);

        drivers::uart::write(header, app::protocol::HEADER_SIZE);
        drivers::uart::write(reinterpret_cast<const uint8_t*>(frame), sizeof(frame));
        drivers::uart::write(footer, app::protocol::FOOTER_SIZE);
        bsp::toggle_led();
    }
}
