#include "bsp/bsp.hpp"
#include "drivers/tim.hpp"
#include "stm32f3xx_hal.h"

namespace {
    TIM_HandleTypeDef htim{};
    constexpr uint32_t TIMER_BASE_HZ = 1'000'000;
    constexpr uint32_t PSC = 71;
}


namespace drivers::tim {
 void init(uint32_t sample_rate_hz) {
if(sample_rate_hz == 0) {
    bsp::error_handler();
}
__HAL_RCC_TIM6_CLK_ENABLE();
htim.Instance = TIM6;
htim.Init.Prescaler = PSC;
htim.Init.Period = TIMER_BASE_HZ / sample_rate_hz - 1;
htim.Init.CounterMode = TIM_COUNTERMODE_UP;
htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

if (HAL_TIM_Base_Init(&htim) != HAL_OK) {
    bsp::error_handler();
}

TIM_MasterConfigTypeDef cfg{};
cfg.MasterOutputTrigger = TIM_TRGO_UPDATE;
cfg.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

if(HAL_TIMEx_MasterConfigSynchronization(&htim, &cfg) != HAL_OK) {
    bsp::error_handler();
}
 }

 void start() {
     HAL_TIM_Base_Start(&htim);
 }

 void stop() {
     HAL_TIM_Base_Stop(&htim);
 }
}
