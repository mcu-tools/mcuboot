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
#include "watchdog.h"

/* Define pins for UART debug output */

#define CY_DEBUG_UART_TX (P5_1)
#define CY_DEBUG_UART_RX (P5_0)

#if defined(PSOC_062_2M)
#warning "Check if User LED is correct for your target board."
#define LED_PORT GPIO_PRT13
#define LED_PIN 7U
#elif defined(PSOC_062_1M)
#define LED_PORT GPIO_PRT13
#define LED_PIN 7U
#elif defined(PSOC_062_512K)
#define LED_PORT GPIO_PRT11
#define LED_PIN 1U
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

#define WATCHDOG_UPD_MESSAGE  "[BlinkyApp] Update watchdog timer started in MCUBootApp to mark successful start of user app\r\n"
#define WATCHDOG_FREE_MESSAGE "[BlinkyApp] Turn off watchdog timer\r\n"

#ifdef BOOT_IMG
    #define BLINK_PERIOD          (1000u)
    #define GREETING_MESSAGE_VER  "[BlinkyApp] BlinkyApp v1.0 [CM4]\r\n"
    #define GREETING_MESSAGE_INFO "[BlinkyApp] Red led blinks with 1 sec period\r\n"
#elif defined(UPGRADE_IMG)
    #define BLINK_PERIOD          (250u)
    #define GREETING_MESSAGE_VER  "[BlinkyApp] BlinkyApp v2.0 [+]\r\n"
    #define GREETING_MESSAGE_INFO "[BlinkyApp] Red led blinks with 0.25 sec period\r\n"
#else
    #error "[BlinkyApp] Please specify type of image: -DBOOT_IMG or -DUPGRADE_IMG\r\n"
#endif

void check_result(int res)
{
    if (res != CY_RSLT_SUCCESS) {
        CY_ASSERT(0);
    }
}

/*
* Writes 1 byte `src` into flash memory at `address`
* It does a sequence of RD/Modify/WR of data in a Flash Row.
 */
int flash_write_byte(uint32_t address, uint8_t src)
{
    cy_en_flashdrv_status_t rc = CY_FLASH_DRV_SUCCESS;
    uint32_t row_addr = 0;
    uint8_t row_buff[512];

    /* accepting arbitrary address */
    row_addr = (address/CY_FLASH_SIZEOF_ROW)*CY_FLASH_SIZEOF_ROW;

    /* preserving Row */
    memcpy(row_buff, (void *)row_addr, sizeof(row_buff));

    /* Modifying the target byte */
    row_buff[address%CY_FLASH_SIZEOF_ROW] = src;

    /* Programming updated row back */
    rc = Cy_Flash_WriteRow(row_addr, (const uint32_t *)row_buff);

    return (int) rc;
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

    /* Update watchdog timer to mark successful start up of application */
    printf(WATCHDOG_UPD_MESSAGE);
    cy_wdg_kick();
    printf(WATCHDOG_FREE_MESSAGE);
    cy_wdg_free();

#if defined(SWAP_ENABLED) && defined(UPGRADE_IMG)

    #define USER_SWAP_IMAGE_OK_OFFS (24)
    #define USER_SWAP_IMAGE_OK      (1)
    uint32_t img_ok_addr;
    int rc;

    printf("[BlinkyApp] Try to set img_ok to confirm upgrade image\r\n");

    /* Write Image OK flag to the slot trailer, so MCUBoot-loader
     * will not revert new image */
    img_ok_addr = USER_APP_START + USER_APP_SIZE - USER_SWAP_IMAGE_OK_OFFS;
    if (*((uint8_t *)img_ok_addr) != USER_SWAP_IMAGE_OK)
    {
        rc = flash_write_byte(img_ok_addr, USER_SWAP_IMAGE_OK);
        if (0 == rc)
        {
            printf("[BlinkyApp] SWAP Status : Image OK was set at 0x%08lx.\r\n", img_ok_addr);
        }
        else
        {
            printf("[BlinkyApp] SWAP Status : Failed to set Image OK.\r\n");
        }
    } else
    {
        printf("[BlinkyApp] Img_ok is already set in trailer\r\n");
    }
#endif

    for (;;)
    {
        /* Toggle the user LED periodically */
        Cy_SysLib_Delay(blinky_period/2);

        /* Invert the USER LED state */
        Cy_GPIO_Inv(LED_PORT, LED_PIN);
    }
    return 0;
}
