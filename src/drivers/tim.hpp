#pragma once
#include "stm32f3xx_hal.h"
#include <cstdint>
namespace drivers {

class BasicTimer {
public:
  explicit BasicTimer(TIM_TypeDef *instance);
  void init(uint32_t sample_rate_hz);
  void start();
  void stop();
  [[nodiscard]] uint32_t set_rate(uint32_t target_rate);
private:
  TIM_HandleTypeDef htim_{};
};

} // namespace drivers
