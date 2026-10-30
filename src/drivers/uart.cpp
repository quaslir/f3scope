#include "drivers/uart.hpp"
#include "bsp/board.hpp"
#include "bsp/bsp.hpp"
#include "stm32f3xx.h"
#include <cstdint>

namespace {
UART_HandleTypeDef huart{};
uint8_t rx_byte{};
uint8_t rx_buf[64];
volatile uint8_t rx_head{};
volatile uint8_t rx_tail{};
} // namespace

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
  huart.Init.BaudRate = baud_rate;
  huart.Init.StopBits = UART_STOPBITS_1;
  huart.Init.WordLength = UART_WORDLENGTH_8B;
  huart.Init.Parity = UART_PARITY_NONE;
  huart.Init.Mode = UART_MODE_TX_RX;
  huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart) != HAL_OK) {
    bsp::error_handler();
  }
  HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);

  if (HAL_UART_Receive_IT(&huart, &rx_byte, 1) != HAL_OK) {
    bsp::error_handler();
  }
}
void write(const uint8_t *data, uint16_t length) {
  HAL_UART_Transmit(&huart, data, length, HAL_MAX_DELAY);
}

bool read_byte(uint8_t &out) {
  if (rx_head == rx_tail)
    return false;
  out = rx_buf[rx_tail];
  rx_tail = (rx_tail + 1) % sizeof(rx_buf);
  return true;
}

extern "C" {
void USART2_IRQHandler() { HAL_UART_IRQHandler(&huart); }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *) {
  uint8_t new_head = (rx_head + 1) % sizeof(rx_buf);
  if (new_head != rx_tail) {
    rx_buf[rx_head] = rx_byte;
    rx_head = new_head;
  }

  HAL_UART_Receive_IT(&huart, &rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *) {
  HAL_UART_Receive_IT(&huart, &rx_byte, 1);
}
}
} // namespace drivers::uart
