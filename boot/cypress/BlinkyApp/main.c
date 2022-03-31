/*
 * Copyright (c) 2020 Cypress Semiconductor Corporation
 * Copyright (c) 2021 Infineon Technologies AG
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

#ifdef CYW20829
#include <inttypes.h>
#include "cybsp.h"
#include "cycfg_pins.h"
#include "cyhal_wdt.h"
#else
#include "system_psoc6.h"
#endif /* CYW20829 */

#include "cy_pdl.h"
#include "cy_retarget_io.h"
#include "cyhal.h"
#include "watchdog.h"

#include "flash_qspi.h"

#if !(SWAP_DISABLED) && defined(UPGRADE_IMAGE)
#include "set_img_ok.h"
#endif

/* Define pins for UART debug output */
#ifdef CYW20829
#define CY_DEBUG_UART_TX (CYBSP_DEBUG_UART_TX)
#define CY_DEBUG_UART_RX (CYBSP_DEBUG_UART_RX)
#else
#define CY_DEBUG_UART_TX (P5_1)
#define CY_DEBUG_UART_RX (P5_0)
#endif /* CYW20829 */

#if defined(PSOC_062_2M)
#define LED_PORT GPIO_PRT13
#define LED_PIN 7U
#elif defined(PSOC_062_1M)
#define LED_PORT GPIO_PRT13
#define LED_PIN 7U
#elif defined(PSOC_062_512K)
#define LED_PORT GPIO_PRT11
#define LED_PIN 1U
#elif defined(CYW20829)
#define LED_PORT GPIO_PRT0
#define LED_PIN 0U
#endif

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

uint32_t smif_id = 1; /* Assume SlaveSelect_0 is used for External Memory */

#ifdef BOOT_IMAGE
    #define BLINK_PERIOD          (1000u)
    #define GREETING_MESSAGE_VER  "[BlinkyApp] BlinkyApp v1.0 [CM4]\r\n"
    #define GREETING_MESSAGE_INFO "[BlinkyApp] Red led blinks with 1 sec period\r\n"
#elif defined(UPGRADE_IMAGE)
    #define BLINK_PERIOD          (250u)
    #define GREETING_MESSAGE_VER  "[BlinkyApp] BlinkyApp v2.0 [+]\r\n"
    #define GREETING_MESSAGE_INFO "[BlinkyApp] Red led blinks with 0.25 sec period\r\n"
#else
    #error "[BlinkyApp] Please specify type of image: -DBOOT_IMAGE or -DUPGRADE_IMAGE\r\n"
#endif

#define WATCHDOG_FREE_MESSAGE "[BlinkyApp] Turn off watchdog timer\r\n"

static void check_result(int res)
{
    if (res != CY_RSLT_SUCCESS) {
        CY_ASSERT(0);
        /* Loop forever... */
        for (;;) {}
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

#ifdef CYW20829
    cy_en_smif_status_t rc = CY_SMIF_CMD_NOT_FOUND;

    rc = qspi_init_sfdp(smif_id);
    if (CY_SMIF_SUCCESS == rc) {
        printf("[BlinkyApp] External Memory initialized w/ SFDP. \r\n");
    }
    else {
        printf("[BlinkyApp] External Memory initialization w/ SFDP FAILED: 0x%" PRIx32 " \r\n", (uint32_t)rc);
    }
#endif /* CYW20829 */
}

int main(void)
{
    uint32_t blinky_period = BLINK_PERIOD;

#if defined CYW20829
    cybsp_init();
#endif /* CYW20829 */

    test_app_init_hardware();

    printf(GREETING_MESSAGE_INFO);

    /* Disable watchdog timer to mark successful start up of application.
     * For PSOC6 WDT is disabled in SystemInit() function.
     */
    printf(WATCHDOG_FREE_MESSAGE);
#ifdef CYW20829
    cyhal_wdt_t *cyw20829_wdt = NULL;
    cyhal_wdt_free(cyw20829_wdt);
#else
    cy_wdg_free();
#endif /* CYW20829 */

#if !(SWAP_DISABLED) && defined(UPGRADE_IMAGE)
    int rc = -1;

    printf("[BlinkyApp] Try to set img_ok to confirm upgrade image\r\n");

    /* Write Image OK flag to the slot trailer, so MCUBoot-loader
     * will not revert new image
     */
    rc = set_img_ok(IMG_OK_ADDR, USER_SWAP_IMAGE_OK);

    if (IMG_OK_ALREADY_SET == rc) {
        printf("[BlinkyApp] Img_ok is already set in trailer\r\n");
    }
    else if (IMG_OK_SET_SUCCESS == rc) {
        printf("[BlinkyApp] SWAP Status : Image OK was set at 0x%08x.\r\n", IMG_OK_ADDR);
    }
    else {
        printf("[BlinkyApp] SWAP Status : Failed to set Image OK.\r\n");
    }

#endif /* !(SWAP_DISABLED) && defined(UPGRADE_IMAGE) */

    for (;;)
    {
        /* Toggle the user LED periodically */
        Cy_SysLib_Delay(blinky_period/2);

        /* Invert the USER LED state */
        Cy_GPIO_Inv(LED_PORT, LED_PIN);
    }

    return 0;
}
