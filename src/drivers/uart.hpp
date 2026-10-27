
#include <cstdint>
namespace drivers::uart {
    void init();
    void write(const uint8_t* data, uint16_t length);
}
