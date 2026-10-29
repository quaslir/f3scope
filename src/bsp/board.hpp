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
    inline GPIO_TypeDef* const ADC_PORT = GPIOA;
    inline constexpr uint16_t ADC_PIN = GPIO_PIN_0;
    inline constexpr uint32_t ADC_CHANNEL = ADC_CHANNEL_1;
    inline ADC_TypeDef* const ADC_INSTANCE = ADC1;
    inline GPIO_TypeDef* const DAC_PORT = GPIOA;
    inline constexpr uint16_t DAC_PIN = GPIO_PIN_4;
    inline DAC_TypeDef* const  DAC_INSTANCE = DAC1;
    inline constexpr uint16_t DAC_CHANNEL = DAC_CHANNEL_1;
    inline  TIM_TypeDef * const ADC_TIMER = TIM6;
    inline  TIM_TypeDef * const DAC_TIMER = TIM7;
}
