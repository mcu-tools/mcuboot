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
#include <stdbool.h>

/* Cypress pdl headers */
#include "cy_pdl.h"

#ifdef CYW20829
#include "cy_retarget_io.h"
#include "cybsp.h"
#include "cyhal_wdt.h"
#include "cyw_platform_utils.h"
#include "cy_service_app.h"
#else
#include "cy_retarget_io_pdl.h"
#include "cycfg_clocks.h"
#include "cycfg_peripherals.h"
#if defined APP_CM0P || defined CM4
#include "cyw_platform_utils.h"
#endif /* defined APP_CM0P || defined CM4  */
#endif /* defined CYW20829 */

#if defined(CY_BOOT_USE_EXTERNAL_FLASH) || defined(CYW20829)
#include "flash_qspi.h"
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) || defined(CYW20829) */

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

#ifdef USE_EXEC_TIME_CHECK
#include "misc/timebase_us.h"
#include "misc/exec_time_check.h"
#endif /* USE_EXEC_TIME_CHECK */

#ifdef USE_LOG_TIMESTAMP
#include "timestamp.h"
#endif /* USE_LOG_TIMESTAMP */

#define CY_RSLT_MODULE_MCUBOOTAPP       0x500U
#define CY_RSLT_MODULE_MCUBOOTAPP_MAIN  0x51U

/** General module error */
#define MCUBOOTAPP_RSLT_ERR                     \
    (CY_RSLT_CREATE_EX(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_MCUBOOTAPP, CY_RSLT_MODULE_MCUBOOTAPP_MAIN, 0))

/* WDT time out for reset mode, in milliseconds. */
#define WDT_TIME_OUT_MS 4000

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
/* Choose SMIF slot number (slave select).
 * Acceptable values are:
 * 0 - SMIF disabled (no external memory);
 * 1, 2, 3 or 4 - slave select line memory module is connected to.
 */
#define SMIF_ID         (1U) /* Assume SlaveSelect_0 is used for External Memory */
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */

#define BOOT_MSG_FINISH "MCUBoot Bootloader finished.\n" \
                        "Deinitializing hardware..."

static void hw_deinit(void);

static inline __attribute__((always_inline))
fih_uint calc_app_addr(uintptr_t flash_base, const struct boot_rsp *rsp)
{
    return fih_uint_encode(flash_base +
                           rsp->br_image_off +
                           rsp->br_hdr->ih_hdr_size);
}

#ifdef CYW20829

#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP)
CY_RAMFUNC_BEGIN /* SMIF will be deinitialized in this case! */
#else
inline __attribute__((always_inline))
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) */
__NO_RETURN
static void cyw20829_launch_app(fih_uint app_addr, uint32_t *key, uint32_t *iv)
{
#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP)
    qspi_deinit(SMIF_ID);
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) */
    platform_RunNextApp(app_addr, key, iv);
}
#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP)
CY_RAMFUNC_END /* SMIF will be deinitialized in this case! */
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) */

#endif /* CYW20829 */

static bool do_boot(struct boot_rsp *rsp)
{
    uintptr_t flash_base = 0;

#ifdef CYW20829
    uint32_t *key = NULL;
    uint32_t *iv = NULL;
#endif /* CYW20829 */

    if (rsp != NULL) {
        int rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);

        if (0 == rc) {
            fih_uint app_addr = calc_app_addr(flash_base, rsp);

            BOOT_LOG_INF("Starting User Application (wait)...");
            if (IS_ENCRYPTED(rsp->br_hdr)) {
                BOOT_LOG_DBG(" * User application is encrypted");
            }
            BOOT_LOG_INF("Start slot Address: 0x%08" PRIx32, (uint32_t)fih_uint_decode(app_addr));

            rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
            if ((rc != 0) || fih_uint_not_eq(calc_app_addr(flash_base, rsp), app_addr)) {
                return false;
            }

#ifdef CYW20829
#ifdef MCUBOOT_ENC_IMAGES_XIP
            if (IS_ENCRYPTED(rsp->br_hdr)) {
                key = rsp->xip_key;
                iv = rsp->xip_iv;
            } else {
                BOOT_LOG_ERR("User image is not encrypted, while MCUBootApp is compiled with encryption support.");
                return false;
            }
#endif /* MCUBOOT_ENC_IMAGES_XIP */


#ifdef APP_CM33
            /* This function does not return */
            BOOT_LOG_INF("Launching app on CM33 core");
            BOOT_LOG_INF(BOOT_MSG_FINISH);
            hw_deinit();
            cyw20829_launch_app(app_addr, key, iv);
#else
#error "Application should run on Cortex-M33"
#endif /* APP_CM33 */

#else /* defined CYW20829 */

#ifdef USE_XIP
            BOOT_LOG_DBG("XIP: Switch to SMIF XIP mode");
            qspi_set_mode(CY_SMIF_MEMORY);
#endif /* USE_XIP */

#ifdef APP_CM4
            /* This function turns on CM4 and returns */
            BOOT_LOG_INF("Launching app on CM4 core");
            BOOT_LOG_INF(BOOT_MSG_FINISH);
            hw_deinit();
#ifdef CM0P
            Cy_SysEnableCM4(fih_uint_decode(app_addr));
            return true;
#else
            psoc6_launch_cm4_app(app_addr);
#endif /* CM0P */

#elif defined APP_CM0P
#ifdef CM0P
            /* This function does not return */
            BOOT_LOG_INF("Launching app on CM0P core");
            BOOT_LOG_INF(BOOT_MSG_FINISH);
            hw_deinit();
            psoc6_launch_cm0p_app(app_addr);
#else
#error "Application should run on Cortex-M4"
#endif /* CM0P */

#else
#error "Application should run on either Cortex-M0+ or Cortex-M4"
#endif /* APP_CM4 */

#endif /* defined CYW20829 */
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
    cy_rslt_t rc = MCUBOOTAPP_RSLT_ERR;
    bool boot_succeeded = false;
    fih_int fih_rc = FIH_FAILURE;

#ifdef CYW20829
    rc = cybsp_init();
    if (rc != CY_RSLT_SUCCESS) {
        CY_ASSERT((bool)0);
        /* Loop forever... */
        while (true) {
            __WFI();
        }
    }
#else
    SystemInit();
    init_cycfg_peripherals();
    init_cycfg_pins();
#endif /* CYW20829 */

#ifdef USE_EXEC_TIME_CHECK
    timebase_us_init();
#endif /* USE_EXEC_TIME_CHECK */

#ifdef USE_LOG_TIMESTAMP
    log_timestamp_init();
#endif /* USE_LOG_TIMESTAMP */

    /* enable interrupts */
    __enable_irq();

    /* Certain PSoC 6 devices enable CM4 by default at startup. It must be
     * either disabled or enabled & running a valid application for flash write
     * to work from CM0+. Since flash write may happen in boot_go() for updating
     * the image before this bootloader app can enable CM4 in do_boot(), we need
     * to keep CM4 disabled. Note that debugging of CM4 is not supported when it
     * is disabled.
     */
#if !defined CYW20829
#if defined(CY_DEVICE_PSOC6ABLE2) && !defined(CM4)
    if (CY_SYS_CM4_STATUS_ENABLED == Cy_SysGetCM4Status()) {
        Cy_SysDisableCM4();
    }
#endif /* defined(CY_DEVICE_PSOC6ABLE2) && !defined(CM4) */
    /* Initialize retarget-io to use the debug UART port (CYBSP_UART_HW) */
    rc = cy_retarget_io_pdl_init(CY_RETARGET_IO_BAUDRATE);
#else
    /* Initialize retarget-io to use the debug UART port */
    rc = cy_retarget_io_init(CYBSP_DEBUG_UART_TX,
                             CYBSP_DEBUG_UART_RX,
                             CY_RETARGET_IO_BAUDRATE);
#endif /* CYW20829 */
    if (rc != CY_RSLT_SUCCESS) {
        CY_ASSERT((bool)0);
        /* Loop forever... */
        while (true) {
            __WFI();
        }
    }

    BOOT_LOG_INF("MCUBoot Bootloader Started");

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    {
        cy_en_smif_status_t qspi_status = qspi_init_sfdp(SMIF_ID);

        if (CY_SMIF_SUCCESS == qspi_status) {
            rc = CY_RSLT_SUCCESS;
            BOOT_LOG_INF("External Memory initialized w/ SFDP.");
        } else {
            rc = MCUBOOTAPP_RSLT_ERR;
            BOOT_LOG_ERR("External Memory initialization w/ SFDP FAILED: 0x%08" PRIx32, (uint32_t)qspi_status);
        }
    }

    if (CY_RSLT_SUCCESS == rc)
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */
    {
#if defined(CYW20829) && defined(MCUBOOT_HW_ROLLBACK_PROT)
        /* Check service application completion status */
        if (check_service_app_status() != 0) {
            BOOT_LOG_ERR("Service application failed");
            CY_ASSERT((bool)0);
            /* Loop forever... */
            while (true) {
                __WFI();
            }
        }
#endif /* CYW20829 && MCUBOOT_HW_ROLLBACK_PROT */

        (void)memset(&rsp, 0, sizeof(rsp));
#ifdef USE_EXEC_TIME_CHECK
        {
            uint32_t exec_time;
            EXEC_TIME_CHECK_BEGIN(&exec_time);
#endif /* USE_EXEC_TIME_CHECK */
                FIH_CALL(boot_go, fih_rc, &rsp);
#ifdef USE_EXEC_TIME_CHECK
            EXEC_TIME_CHECK_END();
            BOOT_LOG_INF("Exec time: %" PRIu32 " [ms]", exec_time / 1000U);
        }
#endif /* USE_EXEC_TIME_CHECK */
        if (true == fih_eq(fih_rc, FIH_SUCCESS)) {
            BOOT_LOG_INF("User Application validated successfully");
            /* initialize watchdog timer. it should be updated from user app
            * to mark successful start up of this app. if the watchdog is not updated,
            * reset will be initiated by watchdog timer and swap revert operation started
            * to roll back to operable image.
            */
#ifdef CYW20829
            cyhal_wdt_t *cyw20829_wdt = NULL;

            rc = cyhal_wdt_init(cyw20829_wdt, WDT_TIME_OUT_MS);
#else
            rc = cy_wdg_init(WDT_TIME_OUT_MS);
#endif /* CYW20829 */
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
}

static void hw_deinit(void)
{
#ifdef CYW20829
    /* Flush the TX buffer, need to be fixed in retarget_io */
    Cy_SysLib_Delay(50);

    cy_retarget_io_deinit();
    cy_wdg_stop();
    cy_wdg_free();
    /* Note: qspi_deinit() is called (if needed) in cyw20829_launch_app() above */
#else
    cy_retarget_io_wait_tx_complete(CYBSP_UART_HW, 10);
    cy_retarget_io_pdl_deinit();
    Cy_GPIO_Port_Deinit(CYBSP_UART_RX_PORT);
    Cy_GPIO_Port_Deinit(CYBSP_UART_TX_PORT);
#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) && !defined(USE_XIP)
    qspi_deinit(SMIF_ID);
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) */
#endif /* CYW20829 */

#ifdef USE_EXEC_TIME_CHECK
    timebase_us_deinit();
#endif /* USE_EXEC_TIME_CHECK */

#ifdef USE_LOG_TIMESTAMP
    log_timestamp_deinit();
#endif /* USE_LOG_TIMESTAMP */
}
