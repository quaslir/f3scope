
#include "bsp/bsp.hpp"
#include "stm32f3xx.h"
#include "bsp/board.hpp"
#include <cstdint>

namespace {
    UART_HandleTypeDef huart{};
}

namespace drivers::uart {

void init(uint32_t baud_rate) {
__HAL_RCC_USART2_CLK_ENABLE();
__HAL_RCC_GPIOA_CLK_ENABLE();


GPIO_InitTypeDef gpio{};
gpio.Pin = bsp::board::UART_TX_PIN | bsp::board::UART_RX_PIN;
gpio.Mode = GPIO_MODE_AF_PP;
gpio.Pull = GPIO_NOPULL;
gpio.Speed = GPIO_SPEED_FREQ_HIGH;
gpio.Alternate = bsp::board::UART_AF;
HAL_GPIO_Init(bsp::board::UART_PORT, &gpio);
huart.Instance = bsp::board::UART_INSTANCE;
huart.Init.BaudRate =baud_rate;
huart.Init.StopBits = UART_STOPBITS_1;
huart.Init.WordLength = UART_WORDLENGTH_8B;
huart.Init.Parity = UART_PARITY_NONE;
huart.Init.Mode = UART_MODE_TX_RX;
huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
huart.Init.OverSampling = UART_OVERSAMPLING_16;

if(HAL_UART_Init(&huart) != HAL_OK) {
    bsp::error_handler();
}


}
void write(const uint8_t* data, uint16_t length) {
    HAL_UART_Transmit(&huart, data, length, HAL_MAX_DELAY);
}
}
