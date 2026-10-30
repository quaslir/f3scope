#include "drivers/tim.hpp"
#include "bsp/board.hpp"
#include "bsp/bsp.hpp"
#include "stm32f3xx_hal.h"
#include "app/timer_match.hpp"
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
    auto calculations = app::timer_math::calculate(TIMER_CLOCK_HZ,target_rate);
    if(!calculations) {
        bsp::error_handler();
    }
  __HAL_TIM_SET_PRESCALER(&htim_, calculations->psc);
  __HAL_TIM_SET_AUTORELOAD(&htim_, calculations->arr);
  if (HAL_TIM_GenerateEvent(&htim_, TIM_EVENTSOURCE_UPDATE) != HAL_OK) {
    bsp::error_handler();
  }
return calculations->sample_rate;
}
void BasicTimer::start() { HAL_TIM_Base_Start(&htim_); }

void BasicTimer::stop() { HAL_TIM_Base_Stop(&htim_); }
} // namespace drivers
