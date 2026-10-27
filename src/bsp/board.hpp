#pragma once

#include "stm32f3xx.h"
namespace bsp::board {
    inline GPIO_TypeDef*  const LD2_PORT = GPIOA;
    inline constexpr uint16_t LD2_PIN = GPIO_PIN_5;
    inline GPIO_TypeDef* const B1_PORT = GPIOC;
    inline constexpr uint16_t B1_PIN = GPIO_PIN_13;
}
