#include "drivers/tim.hpp"
#include "bsp/board.hpp"
#include "bsp/bsp.hpp"
#include "stm32f3xx_hal.h"

namespace drivers {
constexpr uint32_t TIMER_CLOCK_HZ = 72'000'000;
BasicTimer::BasicTimer(TIM_TypeDef *instance) { htim_.Instance = instance; }
void BasicTimer::init(uint32_t sample_rate_hz) {

  if (htim_.Instance == bsp::board::ADC_TIMER) {
    __HAL_RCC_TIM6_CLK_ENABLE();
  } else if (htim_.Instance == bsp::board::DAC_TIMER) {
    __HAL_RCC_TIM7_CLK_ENABLE();
  }

  htim_.Init.Prescaler = 0;
  htim_.Init.Period = 0;
  htim_.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim_.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&htim_) != HAL_OK) {
    bsp::error_handler();
  }

  TIM_MasterConfigTypeDef cfg{};
  cfg.MasterOutputTrigger = TIM_TRGO_UPDATE;
  cfg.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

  if (HAL_TIMEx_MasterConfigSynchronization(&htim_, &cfg) != HAL_OK) {
    bsp::error_handler();
  }

  [[maybe_unused]] auto actual_rate = set_rate(sample_rate_hz);
}
uint32_t BasicTimer::set_rate(uint32_t target_rate) {
  if (target_rate == 0) {
    bsp::error_handler();
  }
  auto ticks = TIMER_CLOCK_HZ / target_rate;
  if (ticks == 0) {
    bsp::error_handler();
  }
  auto psc = (ticks - 1) / 65536;
  auto arr = ticks / (psc + 1) - 1;
  __HAL_TIM_SET_PRESCALER(&htim_, psc);
  __HAL_TIM_SET_AUTORELOAD(&htim_, arr);
  if (HAL_TIM_GenerateEvent(&htim_, TIM_EVENTSOURCE_UPDATE) != HAL_OK) {
    bsp::error_handler();
  }
  return TIMER_CLOCK_HZ / ((psc + 1) * (arr + 1));
}
void BasicTimer::start() { HAL_TIM_Base_Start(&htim_); }

void BasicTimer::stop() { HAL_TIM_Base_Stop(&htim_); }
} // namespace drivers
