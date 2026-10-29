#pragma once
#include <cstdint>
#include "stm32f3xx_hal.h"
namespace drivers  {

    class BasicTimer {
        public:
            explicit BasicTimer(TIM_TypeDef* instance);
            void init(uint32_t sample_rate_hz);
            void start();
            void stop();
        private:
            TIM_HandleTypeDef htim_{};
    };



}
