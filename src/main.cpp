#include "app/commands.hpp"
#include "app/protocol.hpp"
#include "app/trigger.hpp"
#include "app/wavegen.hpp"
#include "bsp/board.hpp"
#include "bsp/bsp.hpp"
#include "drivers/adc.hpp"
#include "drivers/dac.hpp"
#include "drivers/tim.hpp"
#include "drivers/uart.hpp"
#include <cinttypes>
#include <cstdio>
namespace {
constexpr uint32_t UART_BAUDRATE = 1152000;
constexpr uint16_t CAPTURE_SIZE = 2000;
constexpr uint16_t WINDOW = 1000;
constexpr uint8_t PRETRIGGER = 100;
constexpr uint8_t TRIG_HYST = 50;
constexpr uint8_t SINE_POINTS = 100;
drivers::BasicTimer adc_timer{bsp::board::ADC_TIMER};
drivers::BasicTimer dac_timer{bsp::board::DAC_TIMER};

uint16_t frame[CAPTURE_SIZE];
uint16_t sine_table[SINE_POINTS];
uint8_t header[app::protocol::HEADER_SIZE];
uint8_t footer[app::protocol::FOOTER_SIZE];

const char *edge_name(app::trigger::Edge e) {
  return e == app::trigger::Edge::Rising ? "rising" : "falling";
}

void print_settings(const app::commands::Settings &s) {
  std::printf("settings: rate=%" PRIu32 " Hz level=%u edge=%s mode=%c\n",
              s.sample_rate_hz, static_cast<unsigned>(s.level),
              edge_name(s.edge), static_cast<char>(s.mode));
}
} // namespace

int main() {

  app::commands::Settings settings;
  app::commands::Parser parser{settings};
  bsp::init();
  drivers::uart::init(UART_BAUDRATE);
  drivers::adc::init();
  drivers::dac::init();

  adc_timer.init(settings.sample_rate_hz);
  dac_timer.init(100'000);

  app::wavegen::fill_sine(sine_table, SINE_POINTS, 2048, 1700);

  drivers::dac::start_wave(sine_table, SINE_POINTS);
  dac_timer.start();
  std::printf("f3scope ready\n");
  print_settings(settings);
  uint8_t byte;
  while (true) {

      bool settings_changed = false;
      while (drivers::uart::read_byte(byte)) {
        const auto res = parser.feed(static_cast<char>(byte));
        if (res == app::commands::Result::Ok) {
          settings_changed = true;
        } else if (res != app::commands::Result::None) {
          std::printf("cmd error: %s\n", app::commands::to_string(res));
        }
      }

      if (settings_changed) {
        settings.sample_rate_hz = adc_timer.set_rate(settings.sample_rate_hz);
        print_settings(settings);
      }

    drivers::adc::start_capture(frame, CAPTURE_SIZE);
    adc_timer.start();
    while (!drivers::adc::capture_done())
      ; // blocking

    adc_timer.stop();
    drivers::adc::stop_capture();
    auto r = app::trigger::find(frame + PRETRIGGER, CAPTURE_SIZE - WINDOW + 1,
                                settings.level, TRIG_HYST, settings.edge);
    uint16_t start = 0;
    if (r.has_value())
      start = *r;
    const uint16_t checksum = app::protocol::checksum(frame + start, WINDOW);
    app::protocol::write_header(header, WINDOW);
    app::protocol::write_footer(footer, checksum);

    drivers::uart::write(header, app::protocol::HEADER_SIZE);
    drivers::uart::write(reinterpret_cast<const uint8_t *>(frame + start),
                         WINDOW * 2);
    drivers::uart::write(footer, app::protocol::FOOTER_SIZE);
    bsp::toggle_led();
  }
}
