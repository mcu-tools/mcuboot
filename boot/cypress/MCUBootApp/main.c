/***************************************************************************//**
* \file main.c
* \version 1.0
********************************************************************************
* \copyright
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
*******************************************************************************/
#include <inttypes.h>

/* Cypress pdl headers */
#include "cy_pdl.h"
#include "cy_retarget_io_pdl.h"
#include "cycfg_clocks.h"
#include "cycfg_peripherals.h"

#include "flash_qspi.h"

#include "cycfg_pins.h"
#include "cy_result.h"
#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"

#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/sign_key.h"

#include "bootutil/bootutil_log.h"

#include "bootutil/fault_injection_hardening.h"

#include "watchdog.h"

/* WDT time out for reset mode, in milliseconds. */
#define WDT_TIME_OUT_MS 4000

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
/* Choose SMIF slot number (slave select).
 * Acceptable values are:
 * 0 - SMIF disabled (no external memory);
 * 1, 2, 3 or 4 - slave select line memory module is connected to.
 */
uint32_t smif_id = 1; /* Assume SlaveSelect_0 is used for External Memory */
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */

#define BOOT_MSG_FINISH "MCUBoot Bootloader finished.\n" \
                        "Deinitializing hardware..."

static void hw_deinit(void);

static bool do_boot(struct boot_rsp *rsp)
{
    uintptr_t flash_base = 0;

    if (rsp != NULL) {
        int rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);

        if (0 == rc) {
            uint32_t app_addr = flash_base + rsp->br_image_off + rsp->br_hdr->ih_hdr_size;

            BOOT_LOG_INF("Starting User Application (wait)...");
            if (IS_ENCRYPTED(rsp->br_hdr)) {
                BOOT_LOG_DBG(" * User application is encrypted");
            }
            BOOT_LOG_INF("Start slot Address: 0x%08" PRIx32, app_addr);

            /* This function turns on CM4 and returns */
            BOOT_LOG_INF(BOOT_MSG_FINISH);
            hw_deinit();
            Cy_SysEnableCM4(app_addr);
            return true;
        } else {
            BOOT_LOG_ERR("Flash device ID not found");
            return false;
        }
    }

    return false;
}

int main(void)
{
    struct boot_rsp rsp;
    cy_rslt_t rc = CY_RSLT_TYPE_ERROR;
    bool boot_succeeded = false;
    fih_int fih_rc = FIH_FAILURE;

    SystemInit();
    init_cycfg_peripherals();
    init_cycfg_pins();

    /* enable interrupts */
    __enable_irq();

    /* Certain PSoC 6 devices enable CM4 by default at startup. It must be
     * either disabled or enabled & running a valid application for flash write
     * to work from CM0+. Since flash write may happen in boot_go() for updating
     * the image before this bootloader app can enable CM4 in do_boot(), we need
     * to keep CM4 disabled. Note that debugging of CM4 is not supported when it
     * is disabled.
     */
#if defined(CY_DEVICE_PSOC6ABLE2)
    if (CY_SYS_CM4_STATUS_ENABLED == Cy_SysGetCM4Status()) {
        Cy_SysDisableCM4();
    }
#endif /* defined(CY_DEVICE_PSOC6ABLE2) */

    /* Initialize retarget-io to use the debug UART port (CYBSP_UART_HW) */
    rc = cy_retarget_io_pdl_init(CY_RETARGET_IO_BAUDRATE);
    if (rc != CY_RSLT_SUCCESS) {
        CY_ASSERT(0);
        /* Loop forever... */
        while (true) {
            __WFI();
        }
    }

    BOOT_LOG_INF("MCUBoot Bootloader Started");

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    /* enum cy_en_smif_status_t matches cy_rslt_t */
    rc = (cy_rslt_t)qspi_init_sfdp(smif_id);

    if (CY_SMIF_SUCCESS == rc) {
        BOOT_LOG_INF("External Memory initialized w/ SFDP.");
    } else {
        BOOT_LOG_ERR("External Memory initialization w/ SFDP FAILED: 0x%08" PRIx32, rc);
    }

    if (CY_SMIF_SUCCESS == rc)
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */
    {
        (void)memset(&rsp, 0, sizeof(rsp));
        FIH_CALL(boot_go, fih_rc, &rsp);
        if (fih_eq(fih_rc, FIH_SUCCESS)) {
            BOOT_LOG_INF("User Application validated successfully");
            /* initialize watchdog timer. it should be updated from user app
            * to mark successful start up of this app. if the watchdog is not updated,
            * reset will be initiated by watchdog timer and swap revert operation started
            * to roll back to operable image.
            */
            rc = cy_wdg_init(WDT_TIME_OUT_MS);

            if (CY_RSLT_SUCCESS == rc) {
                boot_succeeded = do_boot(&rsp);

                if (!boot_succeeded) {
                    BOOT_LOG_ERR("Boot of next app failed");
                }
            } else {
                BOOT_LOG_ERR("Failed to init WDT");
            }
        } else {
            BOOT_LOG_ERR("MCUBoot Bootloader found none of bootable images");
        }
    }

    while (true) {
        if (boot_succeeded) {
            (void)Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
        } else {
            __WFI();
        }
    }

    return 0;
}

static void hw_deinit(void)
{
    cy_retarget_io_wait_tx_complete(CYBSP_UART_HW, 10);
    cy_retarget_io_pdl_deinit();
    Cy_GPIO_Port_Deinit(CYBSP_UART_RX_PORT);
    Cy_GPIO_Port_Deinit(CYBSP_UART_TX_PORT);
#if defined(CY_BOOT_USE_EXTERNAL_FLASH)
    qspi_deinit(smif_id);
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) */
}
