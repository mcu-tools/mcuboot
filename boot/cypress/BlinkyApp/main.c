/*
 * Copyright (c) 2020 Cypress Semiconductor Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 /*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
 /*******************************************************************************/

#include "system_psoc6.h"
#include "cy_pdl.h"
#include "cyhal.h"
#include "cy_retarget_io.h"

/* Define pins for UART debug output */

#define CY_DEBUG_UART_TX (P5_1)
#define CY_DEBUG_UART_RX (P5_0)

#if defined(PSOC_062_2M)
#warning "Check if User LED is correct for your target board."
#define LED_PORT GPIO_PRT13
#define LED_PIN 7U
#endif

#define LED_NUM 5U
#define LED_DRIVEMODE CY_GPIO_DM_STRONG_IN_OFF
#define LED_INIT_DRIVESTATE 1

const cy_stc_gpio_pin_config_t LED_config =
{
    .outVal = 1,
    .driveMode = CY_GPIO_DM_STRONG_IN_OFF,
    .hsiom = HSIOM_SEL_GPIO,
    .intEdge = CY_GPIO_INTR_DISABLE,
    .intMask = 0UL,
    .vtrip = CY_GPIO_VTRIP_CMOS,
    .slewRate = CY_GPIO_SLEW_FAST,
    .driveSel = CY_GPIO_DRIVE_FULL,
    .vregEn = 0UL,
    .ibufMode = 0UL,
    .vtripSel = 0UL,
    .vrefSel = 0UL,
    .vohSel = 0UL,
};

#ifdef BOOT_IMG
    #define BLINK_PERIOD          (1000u)
    #define GREETING_MESSAGE_VER  "[BlinkyApp] BlinkyApp v1.0 [CM4]\r\n"
    #define GREETING_MESSAGE_INFO "[BlinkyApp] Red led blinks with 1 sec period\r\n"
#elif UPGRADE_IMG
    #define BLINK_PERIOD          (250u)
    #define GREETING_MESSAGE_VER  "[BlinkyApp] BlinkyApp v2.0 [+]\r\n"
    #define GREETING_MESSAGE_INFO "[BlinkyApp] Red led blinks with 0.25 sec period\r\n"
#else
    #error "[BlinkyApp] Please specify type of image: -DBOOT_IMG or -DUPGRADE_IMG\r\n"
#endif

void check_result(int res)
{
    if (res != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
}

void test_app_init_hardware(void)
{
    /* enable interrupts */
    __enable_irq();

    /* Disabling watchdog so it will not interrupt normal flow later */
    Cy_GPIO_Pin_Init(LED_PORT, LED_PIN, &LED_config);
    /* Initialize retarget-io to use the debug UART port */
    check_result(cy_retarget_io_init(CY_DEBUG_UART_TX, CY_DEBUG_UART_RX,
                                     CY_RETARGET_IO_BAUDRATE));

    printf("\n===========================\r\n");
    printf(GREETING_MESSAGE_VER);
    printf("===========================\r\n");

    printf("[BlinkyApp] GPIO initialized \r\n");
    printf("[BlinkyApp] UART initialized \r\n");
    printf("[BlinkyApp] Retarget I/O set to 115200 baudrate \r\n");

}

int main(void)
{
    uint32_t blinky_period = BLINK_PERIOD;

    test_app_init_hardware();

    printf(GREETING_MESSAGE_INFO);

    for (;;)
    {
        /* Toggle the user LED periodically */
        Cy_SysLib_Delay(blinky_period/2);

        /* Invert the USER LED state */
        Cy_GPIO_Inv(LED_PORT, LED_PIN);
    }
    return 0;
}
