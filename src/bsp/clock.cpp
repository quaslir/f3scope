
#include "bsp/clock.hpp"
#include "bsp/bsp.hpp"
#include "stm32f3xx_hal.h"

namespace bsp::clock {
    void init() {
        RCC_OscInitTypeDef osc{};
        osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
        osc.HSEState = RCC_HSE_BYPASS;
        osc.PLL.PLLState = RCC_PLL_ON;
        osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
        osc.PLL.PREDIV = RCC_PREDIV_DIV1;
        osc.PLL.PLLMUL = RCC_PLL_MUL9;

        if(HAL_RCC_OscConfig(&osc) != HAL_OK) {
            error_handler();
        }

        RCC_ClkInitTypeDef clk{};
        clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
        clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
        clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
        clk.APB1CLKDivider = RCC_HCLK_DIV2;
        clk.APB2CLKDivider = RCC_HCLK_DIV1;

     if(HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
         error_handler();
     }
    }
}
