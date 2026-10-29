#include "drivers/dac.hpp"
#include "bsp/board.hpp"
#include "bsp/bsp.hpp"
#include "stm32f3xx_hal.h"

namespace {
    DAC_HandleTypeDef hdac{};
    DMA_HandleTypeDef hdma{};
}

namespace drivers::dac {
    void init() {
        __HAL_RCC_DAC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMA2_CLK_ENABLE();

        GPIO_InitTypeDef gpio{};
        gpio.Pin = bsp::board::DAC_PIN;
        gpio.Mode = GPIO_MODE_ANALOG;
        gpio.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(bsp::board::DAC_PORT, &gpio);


        hdma.Instance = DMA2_Channel3;
        hdma.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma.Init.MemInc = DMA_MINC_ENABLE;
        hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hdma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        hdma.Init.Mode = DMA_CIRCULAR;
        hdma.Init.Priority = DMA_PRIORITY_MEDIUM;

        if(HAL_DMA_Init(&hdma) != HAL_OK) {
            bsp::error_handler();
        }
        __HAL_LINKDMA(&hdac, DMA_Handle1, hdma);

        hdac.Instance = bsp::board::DAC_INSTANCE;

        HAL_NVIC_SetPriority(DMA2_Channel3_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(DMA2_Channel3_IRQn);

        if(HAL_DAC_Init(&hdac) != HAL_OK) {
            bsp::error_handler();
        }
        DAC_ChannelConfTypeDef ch{};
        ch.DAC_Trigger = DAC_TRIGGER_T7_TRGO;
        ch.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

        if(HAL_DAC_ConfigChannel(&hdac, &ch, bsp::board::DAC_CHANNEL) != HAL_OK) {
            bsp::error_handler();
        }


    }

    void set(uint16_t value) {
                if(HAL_DAC_SetValue(&hdac, bsp::board::DAC_CHANNEL, DAC_ALIGN_12B_R, static_cast<uint32_t>(value)) != HAL_OK) {
                    bsp::error_handler();
                }
    }

    void start_wave(const uint16_t* table, uint16_t n) {
        if(HAL_DAC_Start_DMA(&hdac, bsp::board::DAC_CHANNEL, reinterpret_cast<uint32_t*>(const_cast<uint16_t*>(table)), static_cast<uint32_t>(n), DAC_ALIGN_12B_R) != HAL_OK) {
            bsp::error_handler();
        }
    }


    extern "C" {
       void DMA2_Channel3_IRQHandler() {
           HAL_DMA_IRQHandler(&hdma);
       }
    }
}
