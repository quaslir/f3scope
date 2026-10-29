cmake_minimum_required(VERSION 3.22)
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake)
endif()
project(f3scope C CXX ASM)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/STM32F303RETx_FLASH.ld)
set(HAL_DIR ${CMAKE_SOURCE_DIR}/third_party/stm32f3xx_hal_driver)

add_library(hal OBJECT
    ${HAL_DIR}/Src/stm32f3xx_hal.c
    ${HAL_DIR}/Src/stm32f3xx_hal_adc.c
    ${HAL_DIR}/Src/stm32f3xx_hal_adc_ex.c
    ${HAL_DIR}/Src/stm32f3xx_hal_cortex.c
    ${HAL_DIR}/Src/stm32f3xx_hal_dma.c
    ${HAL_DIR}/Src/stm32f3xx_hal_flash.c
    ${HAL_DIR}/Src/stm32f3xx_hal_flash_ex.c
    ${HAL_DIR}/Src/stm32f3xx_hal_gpio.c
    ${HAL_DIR}/Src/stm32f3xx_hal_pwr.c
    ${HAL_DIR}/Src/stm32f3xx_hal_pwr_ex.c
    ${HAL_DIR}/Src/stm32f3xx_hal_rcc.c
    ${HAL_DIR}/Src/stm32f3xx_hal_rcc_ex.c
    ${HAL_DIR}/Src/stm32f3xx_hal_tim.c
    ${HAL_DIR}/Src/stm32f3xx_hal_tim_ex.c
    ${HAL_DIR}/Src/stm32f3xx_hal_uart.c
    ${HAL_DIR}/Src/stm32f3xx_hal_uart_ex.c
    ${HAL_DIR}/Src/stm32f3xx_hal_dac.c
    ${HAL_DIR}/Src/stm32f3xx_hal_dac_ex.c
    startup/system_stm32f3xx.c
)
target_include_directories(hal PUBLIC
    ${CMAKE_SOURCE_DIR}/src                                   # stm32f3xx_hal_conf.h
    ${CMAKE_SOURCE_DIR}/third_party/CMSIS_5/CMSIS/Core/Include
    ${CMAKE_SOURCE_DIR}/third_party/cmsis_device_f3/Include
    ${HAL_DIR}/Inc
)
target_compile_definitions(hal PUBLIC STM32F303xE USE_HAL_DRIVER)

add_executable(firmware
    startup/startup_stm32f303xe.s
    src/main.cpp
    src/bsp/syscalls.cpp
    src/bsp/bsp.cpp
    src/bsp/irq.cpp
    src/bsp/clock.cpp

    src/drivers/uart.cpp
    src/drivers/adc.cpp

    src/drivers/tim.cpp
    src/drivers/dac.cpp

    src/app/wavegen.cpp
)
target_link_libraries(firmware PRIVATE hal)

target_compile_options(firmware PRIVATE
    -Wall -Wextra -Wshadow
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2 -g>
)
target_link_options(firmware PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,-Map=firmware.map
)
set_target_properties(firmware PROPERTIES
    SUFFIX ".elf"
    LINK_DEPENDS ${LINKER_SCRIPT}
)

add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:firmware> firmware.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:firmware> firmware.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware>
)

add_custom_target(flash
    COMMAND openocd -f board/st_nucleo_f3.cfg
            -c "program $<TARGET_FILE:firmware> verify reset exit"
    DEPENDS firmware
    USES_TERMINAL
)
