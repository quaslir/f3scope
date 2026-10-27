#include "bsp/bsp.hpp"
#include "bsp/board.hpp"
#include "stm32f3xx_hal.h"
namespace bsp {
    void init() {
        HAL_Init();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef gpio{};
        gpio.Pin = board::LD2_PIN;
        gpio.Mode = GPIO_MODE_OUTPUT_PP;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(board::LD2_PORT, &gpio);

    }
    void set_led(bool on) {
        HAL_GPIO_WritePin(board::LD2_PORT, board::LD2_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
    void toggle_led() {
        HAL_GPIO_TogglePin(board::LD2_PORT, board::LD2_PIN);
    }
}
