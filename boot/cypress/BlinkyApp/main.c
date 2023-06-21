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

#include "platform.h"

#if !(SWAP_DISABLED) && defined(UPGRADE_IMAGE)
#include "set_img_ok.h"
#endif

int main(void)
{
    const char* detect_core_message = test_app_init_hardware();

    printf(GREETING_MESSAGE_INFO);

#if !(SWAP_DISABLED) && defined(UPGRADE_IMAGE)
    int rc = -1;

    printf("[BlinkyApp] Try to set img_ok to confirm upgrade image\r\n");

    /* Write Image OK flag to the slot trailer, so MCUBoot-loader
     * will not revert new image
     */
    rc = set_img_ok(IMG_OK_ADDR, USER_SWAP_IMAGE_OK);

    if (IMG_OK_ALREADY_SET == rc) {
        printf("[BlinkyApp] Img_ok is already set in trailer\r\n");
    } else if (IMG_OK_SET_SUCCESS == rc) {
        printf("[BlinkyApp] SWAP Status : Image OK was set at 0x%08x.\r\n", IMG_OK_ADDR);
    } else {
        printf("[BlinkyApp] SWAP Status : Failed to set Image OK.\r\n");
    }

#endif /* !(SWAP_DISABLED) && defined(UPGRADE_IMAGE) */

    printf("[BlinkyApp] Image type: " IMAGE_TYPE " on %s core\r\n", detect_core_message);

    /* Disable watchdog timer to mark successful start up of application. */
    cyhal_wdt_free(NULL);

    printf(WATCHDOG_FREE_MESSAGE);

    for (;;) {
        /* Toggle the user LED periodically */
        Cy_SysLib_Delay(BLINK_PERIOD / 2);

        /* Invert the USER LED state */
        Cy_GPIO_Inv(LED_PORT, LED_PIN);
    }

    return 0;
}
