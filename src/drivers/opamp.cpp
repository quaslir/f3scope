#include "drivers/opamp.hpp"
#include "bsp/board.hpp"
#include "bsp/bsp.hpp"
#include "stm32f3xx_hal.h"
namespace {
OPAMP_HandleTypeDef hopamp;
}

namespace drivers::opamp {
void init() {
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef gpio{};
  gpio.Pin = bsp::board::OPAMP_OUT | bsp::board::OPAMP_IN;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(bsp::board::OPAMP_PORT, &gpio);

  hopamp.Instance = bsp::board::OPAMP_INSTANCE;
  hopamp.Init.Mode = OPAMP_FOLLOWER_MODE;
  hopamp.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO0;
  hopamp.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;

  if (HAL_OPAMP_Init(&hopamp) != HAL_OK) {
    bsp::error_handler();
  }
  if (HAL_OPAMP_SelfCalibrate(&hopamp) != HAL_OK) {
    bsp::error_handler();
  }
  if (HAL_OPAMP_Start(&hopamp) != HAL_OK) {
    bsp::error_handler();
  }
}
} // namespace drivers::opamp
