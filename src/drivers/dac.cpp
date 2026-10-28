#include "drivers/dac.hpp"
#include "bsp/board.hpp"
#include "bsp/bsp.hpp"
#include "stm32f3xx_hal.h"

namespace {
    DAC_HandleTypeDef hdac{};
}

namespace drivers::dac {
    void init() {
        __HAL_RCC_DAC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef gpio{};
        gpio.Pin = bsp::board::DAC_PIN;
        gpio.Mode = GPIO_MODE_ANALOG;
        gpio.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(bsp::board::DAC_PORT, &gpio);

        hdac.Instance = bsp::board::DAC_INSTANCE;


        if(HAL_DAC_Init(&hdac) != HAL_OK) {
            bsp::error_handler();
        }
        DAC_ChannelConfTypeDef ch{};
        ch.DAC_Trigger = DAC_TRIGGER_NONE;
        ch.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

        if(HAL_DAC_ConfigChannel(&hdac, &ch, bsp::board::DAC_CHANNEL) != HAL_OK) {
            bsp::error_handler();
        }


        if(HAL_DAC_Start(&hdac, bsp::board::DAC_CHANNEL) != HAL_OK) {
            bsp::error_handler();
        }

    }

    void set(uint16_t value) {
                if(HAL_DAC_SetValue(&hdac, bsp::board::DAC_CHANNEL, DAC_ALIGN_12B_R, static_cast<uint32_t>(value)) != HAL_OK) {
                    bsp::error_handler();
                }
    }
}
