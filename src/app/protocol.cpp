#include "app/protocol.hpp"

namespace app::protocol {

uint16_t checksum(const uint16_t* samples, uint16_t count) {
    uint16_t sum = 0;
    for(uint16_t i = 0; i < count; i++) {
        sum += samples[i];
    }
    return sum;
}
void write_header(uint8_t * out, uint16_t count) {
    out[0] = SYNC0;
    out[1] = SYNC1;
    out[2] = static_cast<uint8_t>(count & 0xFF);
    out[3] = static_cast<uint8_t>(count >> 8);
}
void write_footer(uint8_t*out, uint16_t checksum) {
    out[0] = static_cast<uint8_t>(checksum & 0xFF);
    out[1] = static_cast<uint8_t>(checksum >> 8);
}
}
