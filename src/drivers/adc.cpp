#include "drivers/adc.hpp"
#include "bsp/board.hpp"
#include "bsp/bsp.hpp"
#include "stm32f3xx_hal.h"
namespace drivers::adc {

namespace {
ADC_HandleTypeDef hadc{};
DMA_HandleTypeDef hdma{};
volatile bool frame_ready = false;
} // namespace

void init() {
  __HAL_RCC_ADC12_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef gpio{};
  gpio.Pin = bsp::board::ADC_PIN;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(bsp::board::ADC_PORT, &gpio);

  hadc.Instance = bsp::board::ADC_INSTANCE;
  hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.NbrOfConversion = 1;
  hadc.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T6_TRGO;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;

  // DMA

  __HAL_RCC_DMA1_CLK_ENABLE();

  hdma.Instance = DMA1_Channel1;
  hdma.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma.Init.MemInc = DMA_MINC_ENABLE;
  hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma.Init.Mode = DMA_NORMAL;
  hdma.Init.Priority = DMA_PRIORITY_HIGH;

  if (HAL_DMA_Init(&hdma) != HAL_OK) {
    bsp::error_handler();
  }
  __HAL_LINKDMA(&hadc, DMA_Handle, hdma);
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  if (HAL_ADC_Init(&hadc) != HAL_OK) {
    bsp::error_handler();
  }
  ADC_ChannelConfTypeDef ch{};
  ch.Channel = bsp::board::ADC_CHANNEL;
  ch.Rank = ADC_REGULAR_RANK_1;
  ch.SamplingTime = ADC_SAMPLETIME_61CYCLES_5;
  ch.SingleDiff = ADC_SINGLE_ENDED;
  ch.OffsetNumber = ADC_OFFSET_NONE;
  if (HAL_ADC_ConfigChannel(&hadc, &ch) != HAL_OK) {
    bsp::error_handler();
  }
  if (HAL_ADCEx_Calibration_Start(&hadc, ADC_SINGLE_ENDED) != HAL_OK) {
    bsp::error_handler();
  }
}

uint16_t read() {
  if (HAL_ADC_Start(&hadc) != HAL_OK) {
    bsp::error_handler();
  }
  if (HAL_ADC_PollForConversion(&hadc, 1) != HAL_OK) {
    bsp::error_handler();
  }
  return static_cast<uint16_t>(HAL_ADC_GetValue(&hadc));
}

void start_capture(uint16_t *buffer, uint16_t count) {
  frame_ready = false;
  if (HAL_ADC_Start_DMA(&hadc, reinterpret_cast<uint32_t *>(buffer),
                        static_cast<uint32_t>(count)) != HAL_OK) {
    bsp::error_handler();
  }
}
bool capture_done() { return frame_ready; }

void stop_capture() {
  if (HAL_ADC_Stop_DMA(&hadc) != HAL_OK) {
    bsp::error_handler();
  }
}

extern "C" {
void DMA1_Channel1_IRQHandler() { HAL_DMA_IRQHandler(&hdma); }

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *) { frame_ready = true; }
}
} // namespace drivers::adc
