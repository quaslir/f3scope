#pragma once

#include "stm32f3xx.h"
#include <cstdint>
namespace bsp::board {
    inline GPIO_TypeDef*  const LD2_PORT = GPIOA;
    inline constexpr uint16_t LD2_PIN = GPIO_PIN_5;
    inline GPIO_TypeDef* const B1_PORT = GPIOC;
    inline constexpr uint16_t B1_PIN = GPIO_PIN_13;
    inline USART_TypeDef* const UART_INSTANCE = USART2;
    inline GPIO_TypeDef* const UART_PORT = GPIOA;
    inline constexpr uint16_t UART_TX_PIN  = GPIO_PIN_2;
    inline constexpr uint16_t UART_RX_PIN = GPIO_PIN_3;
    inline constexpr uint8_t UART_AF = GPIO_AF7_USART2;
}
