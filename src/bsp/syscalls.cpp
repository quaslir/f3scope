#include "drivers/uart.hpp"
#include <cstdint>
extern "C" {
int _write(int fd, char *ptr, int len) {
  drivers::uart::write(reinterpret_cast<const uint8_t *>(ptr), len);
  return len;
}
}
