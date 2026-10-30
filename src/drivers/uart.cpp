#include "drivers/uart.hpp"
#include "bsp/board.hpp"
#include "bsp/bsp.hpp"
#include "stm32f3xx.h"
#include <cstdint>

namespace {
UART_HandleTypeDef huart{};
DMA_HandleTypeDef hdma_rx{};
uint8_t rx_buf[256];
uint16_t tail{};
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

  __HAL_RCC_DMA1_CLK_ENABLE();

  hdma_rx.Instance = DMA1_Channel6;
  hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
  hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_rx.Init.Mode = DMA_CIRCULAR;
  hdma_rx.Init.Priority = DMA_PRIORITY_LOW;

  if (HAL_DMA_Init(&hdma_rx) != HAL_OK) {
    bsp::error_handler();
  }
  __HAL_LINKDMA(&huart, hdmarx, hdma_rx);

  if (HAL_UART_Init(&huart) != HAL_OK) {
    bsp::error_handler();
  }
  if (HAL_UART_Receive_DMA(&huart, rx_buf, sizeof(rx_buf)) != HAL_OK) {
    bsp::error_handler();
  }
}
void write(const uint8_t *data, uint16_t length) {
  HAL_UART_Transmit(&huart, data, length, HAL_MAX_DELAY);
}

bool read_byte(uint8_t &out) {
  uint16_t head = sizeof(rx_buf) - __HAL_DMA_GET_COUNTER(&hdma_rx);
  if (head == tail) {
    return false;
  }
  out = rx_buf[tail];
  tail = (tail + 1) % sizeof(rx_buf);
  return true;
}

} // namespace drivers::uart
