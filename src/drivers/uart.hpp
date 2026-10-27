#include <cstdint>
namespace drivers::uart {
    void init(uint32_t baudrate);
    void write(const uint8_t* data, uint16_t length);
}
