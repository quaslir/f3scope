#include "stm32f3xx.h"
#include "stm32f3xx_hal.h"

extern "C" {
void SysTick_Handler() { HAL_IncTick(); }
}
