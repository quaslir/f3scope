#include "bsp/bsp.hpp"
#include "drivers/tim.hpp"
#include "bsp/board.hpp"
#include "stm32f3xx_hal.h"

namespace drivers {
    constexpr uint32_t TIMER_BASE_HZ = 1'000'000;
    constexpr uint32_t PSC = 71;
BasicTimer::BasicTimer(TIM_TypeDef* instance)  {
    htim_.Instance = instance;
}
 void BasicTimer::init(uint32_t sample_rate_hz) {
if(sample_rate_hz == 0) {
    bsp::error_handler();
}
if(htim_.Instance == bsp::board::ADC_TIMER) {
    __HAL_RCC_TIM6_CLK_ENABLE();
} else if(htim_.Instance == bsp::board::DAC_TIMER) {
    __HAL_RCC_TIM7_CLK_ENABLE();
}

htim_.Init.Prescaler = PSC;
htim_.Init.Period = TIMER_BASE_HZ / sample_rate_hz - 1;
htim_.Init.CounterMode = TIM_COUNTERMODE_UP;
htim_.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

if (HAL_TIM_Base_Init(&htim_) != HAL_OK) {
    bsp::error_handler();
}

TIM_MasterConfigTypeDef cfg{};
cfg.MasterOutputTrigger = TIM_TRGO_UPDATE;
cfg.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

if(HAL_TIMEx_MasterConfigSynchronization(&htim_, &cfg) != HAL_OK) {
    bsp::error_handler();
}
 }

 void BasicTimer::start() {
     HAL_TIM_Base_Start(&htim_);
 }

 void BasicTimer::stop() {
     HAL_TIM_Base_Stop(&htim_);
 }
}
