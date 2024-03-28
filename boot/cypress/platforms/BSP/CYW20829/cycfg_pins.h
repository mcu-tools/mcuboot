/*******************************************************************************
* File Name: cycfg_pins.h
*
* Description:
* Pin configuration
* This file was automatically generated and should not be modified.
* Configurator Backend 3.10.0
* device-db 4.100.0.4783
* mtb-pdl-cat1 3.9.0.29592
*
********************************************************************************
* Copyright 2024 Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
********************************************************************************/

#if !defined(CYCFG_PINS_H)
#define CYCFG_PINS_H

#include "cycfg_notices.h"
#include "cy_gpio.h"
#if defined (CY_USING_HAL)
    #include "cyhal_hwmgr.h"
#endif //defined (CY_USING_HAL)
#include "cycfg_routing.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if defined (CY_USING_HAL)
    #define CYBSP_PDM_CLK (P0_0)
    #define CYBSP_D6 CYBSP_PDM_CLK
    #define CYBSP_PDM_DATA (P0_1)
    #define CYBSP_D7 CYBSP_PDM_DATA
    #define CYBSP_D18 (P0_2)
    #define CYBSP_LED_RGB_RED CYBSP_D18
    #define CYBSP_D19 (P0_3)
    #define CYBSP_LED_RGB_GREEN CYBSP_D19
    #define CYBSP_I2S_MCLK (P0_4)
    #define CYBSP_LED_RGB_BLUE CYBSP_I2S_MCLK
    #define CYBSP_USER_BTN (P0_5)
    #define CYBSP_USER_BTN1 CYBSP_USER_BTN
    #define CYBSP_I2S_SCK CYBSP_USER_BTN
    #define CYBSP_USER_BTN2 (P1_0)
    #define CYBSP_D3 CYBSP_USER_BTN2
    #define CYBSP_D10 CYBSP_USER_BTN2
    #define CYBSP_I2S_WS CYBSP_USER_BTN2
    #define CYBSP_SPI_CS CYBSP_USER_BTN2
    #define CYBSP_USER_LED1 (P1_1)
    #define CYBSP_D13 CYBSP_USER_LED1
    #define CYBSP_USER_LED CYBSP_USER_LED1
    #define CYBSP_I2S_DATA CYBSP_USER_LED1
    #define CYBSP_SPI_CLK CYBSP_USER_LED1
    #define CYBSP_SWDIO (P1_2)
    #define CYBSP_D5 CYBSP_SWDIO
    #define CYBSP_D11 CYBSP_SWDIO
    #define CYBSP_SPI_MOSI CYBSP_SWDIO
    #define CYBSP_SWDCK (P1_3)
    #define CYBSP_D4 CYBSP_SWDCK
    #define CYBSP_SPI_MISO CYBSP_SWDCK
    #define CYBSP_D12 CYBSP_SWDCK
    #define CYBSP_D2 (P1_4)
    #define CYBSP_LIN_EN CYBSP_D2
    #define CYBSP_D8 (P1_5)
    #define CYBSP_LIN_RX CYBSP_D8
    #define CYBSP_D9 (P1_6)
    #define CYBSP_LIN_TX CYBSP_D9
    #define CYBSP_QSPI_SS (P2_0)
    #define CYBSP_QSPI_D3 (P2_1)
    #define CYBSP_QSPI_D2 (P2_2)
    #define CYBSP_QSPI_D1 (P2_3)
    #define CYBSP_QSPI_D0 (P2_4)
    #define CYBSP_QSPI_SCK (P2_5)
#endif //defined (CY_USING_HAL)
#define CYBSP_BT_UART_CTS_ENABLED 1U
#define CYBSP_DEBUG_UART_CTS_ENABLED CYBSP_BT_UART_CTS_ENABLED
#define CYBSP_BT_UART_CTS_PORT GPIO_PRT3
#define CYBSP_DEBUG_UART_CTS_PORT CYBSP_BT_UART_CTS_PORT
#define CYBSP_BT_UART_CTS_PORT_NUM 3U
#define CYBSP_DEBUG_UART_CTS_PORT_NUM CYBSP_BT_UART_CTS_PORT_NUM
#define CYBSP_BT_UART_CTS_PIN 0U
#define CYBSP_DEBUG_UART_CTS_PIN CYBSP_BT_UART_CTS_PIN
#define CYBSP_BT_UART_CTS_NUM 0U
#define CYBSP_DEBUG_UART_CTS_NUM CYBSP_BT_UART_CTS_NUM
#define CYBSP_BT_UART_CTS_DRIVEMODE CY_GPIO_DM_ANALOG
#define CYBSP_DEBUG_UART_CTS_DRIVEMODE CYBSP_BT_UART_CTS_DRIVEMODE
#define CYBSP_BT_UART_CTS_INIT_DRIVESTATE 1
#define CYBSP_DEBUG_UART_CTS_INIT_DRIVESTATE CYBSP_BT_UART_CTS_INIT_DRIVESTATE
#ifndef ioss_0_port_3_pin_0_HSIOM
    #define ioss_0_port_3_pin_0_HSIOM HSIOM_SEL_GPIO
#endif
#define CYBSP_BT_UART_CTS_HSIOM ioss_0_port_3_pin_0_HSIOM
#define CYBSP_DEBUG_UART_CTS_HSIOM CYBSP_BT_UART_CTS_HSIOM
#define CYBSP_BT_UART_CTS_IRQ ioss_interrupts_gpio_3_IRQn
#define CYBSP_DEBUG_UART_CTS_IRQ CYBSP_BT_UART_CTS_IRQ
#if defined (CY_USING_HAL)
    #define CYBSP_BT_UART_CTS_HAL_PORT_PIN P3_0
    #define CYBSP_DEBUG_UART_CTS_HAL_PORT_PIN CYBSP_BT_UART_CTS_HAL_PORT_PIN
    #define CYBSP_BT_UART_CTS P3_0
    #define CYBSP_DEBUG_UART_CTS CYBSP_BT_UART_CTS
    #define CYBSP_BT_UART_CTS_HAL_IRQ CYHAL_GPIO_IRQ_NONE
    #define CYBSP_DEBUG_UART_CTS_HAL_IRQ CYBSP_BT_UART_CTS_HAL_IRQ
    #define CYBSP_BT_UART_CTS_HAL_DIR CYHAL_GPIO_DIR_INPUT 
    #define CYBSP_DEBUG_UART_CTS_HAL_DIR CYBSP_BT_UART_CTS_HAL_DIR
    #define CYBSP_BT_UART_CTS_HAL_DRIVEMODE CYHAL_GPIO_DRIVE_ANALOG
    #define CYBSP_DEBUG_UART_CTS_HAL_DRIVEMODE CYBSP_BT_UART_CTS_HAL_DRIVEMODE
    #define CYBSP_BT_UART_RTS (P3_1)
    #define CYBSP_DEBUG_UART_RTS CYBSP_BT_UART_RTS
    #define CYBSP_BT_UART_RX (P3_2)
    #define CYBSP_DEBUG_UART_RX CYBSP_BT_UART_RX
    #define CYBSP_D0 CYBSP_BT_UART_RX
    #define CYBSP_BT_UART_TX (P3_3)
    #define CYBSP_DEBUG_UART_TX CYBSP_BT_UART_TX
    #define CYBSP_D1 CYBSP_BT_UART_TX
    #define CYBSP_THERMISTOR (P3_4)
    #define CYBSP_A0 CYBSP_THERMISTOR
#endif //defined (CY_USING_HAL)
#define CYBSP_A1_ENABLED 1U
#define CYBSP_A1_PORT GPIO_PRT3
#define CYBSP_A1_PORT_NUM 3U
#define CYBSP_A1_PIN 5U
#define CYBSP_A1_NUM 5U
#define CYBSP_A1_DRIVEMODE CY_GPIO_DM_ANALOG
#define CYBSP_A1_INIT_DRIVESTATE 1
#ifndef ioss_0_port_3_pin_5_HSIOM
    #define ioss_0_port_3_pin_5_HSIOM HSIOM_SEL_GPIO
#endif
#define CYBSP_A1_HSIOM ioss_0_port_3_pin_5_HSIOM
#define CYBSP_A1_IRQ ioss_interrupts_gpio_3_IRQn
#if defined (CY_USING_HAL)
    #define CYBSP_A1_HAL_PORT_PIN P3_5
    #define CYBSP_A1 P3_5
    #define CYBSP_A1_HAL_IRQ CYHAL_GPIO_IRQ_NONE
    #define CYBSP_A1_HAL_DIR CYHAL_GPIO_DIR_INPUT 
    #define CYBSP_A1_HAL_DRIVEMODE CYHAL_GPIO_DRIVE_ANALOG
    #define CYBSP_A2 (P3_6)
    #define CYBSP_A3 (P3_7)
    #define CYBSP_I2C_SCL (P4_0)
    #define CYBSP_I2C_SDA (P4_1)
    #define CYBSP_XTAL1 (P5_0)
    #define CYBSP_CAN_RX CYBSP_XTAL1
    #define CYBSP_XTAL0 (P5_1)
    #define CYBSP_CAN_TX CYBSP_XTAL0
    #define CYBSP_USER_LED2 (P5_2)
#endif //defined (CY_USING_HAL)

extern const cy_stc_gpio_pin_config_t CYBSP_BT_UART_CTS_config;
#define CYBSP_DEBUG_UART_CTS_config CYBSP_BT_UART_CTS_config
#if defined (CY_USING_HAL)
    extern const cyhal_resource_inst_t CYBSP_BT_UART_CTS_obj;
    #define CYBSP_DEBUG_UART_CTS_obj CYBSP_BT_UART_CTS_obj
#endif //defined (CY_USING_HAL)
extern const cy_stc_gpio_pin_config_t CYBSP_A1_config;
#if defined (CY_USING_HAL)
    extern const cyhal_resource_inst_t CYBSP_A1_obj;
#endif //defined (CY_USING_HAL)

void init_cycfg_pins(void);
void reserve_cycfg_pins(void);

#if defined(__cplusplus)
}
#endif


#endif /* CYCFG_PINS_H */
