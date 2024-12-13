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
#include "cyhal.h"
#include "cyhal_wdt.h"
#include "cy_wdt.h"

#include "platform_utils.h"

#if defined CYW20829
#include "cy_service_app.h"
#endif

#include "cybsp.h"
#include "cy_retarget_io.h"

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

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
/* Choose SMIF slot number (slave select).
 * Acceptable values are:
 * 0 - SMIF disabled (no external memory);
 * 1, 2, 3 or 4 - slave select line memory module is connected to.
 */
#define SMIF_ID         (1U) /* Assume SlaveSelect_0 is used for External Memory */
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */

#define BOOT_MSG_FINISH "MCUBoot Bootloader finished.\r\n" \
                        "Deinitializing hardware..."

/* WDT time out for reset mode, in milliseconds. */
#define WDT_TIME_OUT_MS                     4000
/* Match count =  Desired interrupt interval in seconds x ILO Frequency in Hz */
#define WDT_MATCH_COUNT                     (WDT_TIME_OUT_MS*32000)/1000

static void hw_deinit(void);

#if defined(USE_WDT_PDL)
cy_rslt_t initialize_wdt()
{
   /* Step 1- Unlock WDT */
   Cy_WDT_Unlock();

   /* Step 2- Write the ignore bits - operate with only 14 bits */
   Cy_WDT_SetIgnoreBits(16);

   /* Step 3- Write match value */
   Cy_WDT_SetMatch(WDT_MATCH_COUNT);

   /* Step 4- Clear match event interrupt, if any */
   Cy_WDT_ClearInterrupt();

   /* Step 5- Enable WDT */
   Cy_WDT_Enable();

   /* Step 6- Lock WDT configuration */
   Cy_WDT_Lock();

   return CY_RSLT_SUCCESS;
}
#endif

static inline __attribute__((always_inline))
fih_uint calc_app_addr(uintptr_t flash_base, const struct boot_rsp *rsp)
{
    return fih_uint_encode(flash_base +
                           rsp->br_image_off +
                           rsp->br_hdr->ih_hdr_size);
}

/*******************************************************************************
 * Function Name: fih_calc_app_addr
 ********************************************************************************
 * Summary:
 * Calculate start address of user application.
 *
 * Parameters:
 *  image_base - base address of flash;
 *
 *  rsp - provided by the boot loader code; indicates where to jump
 *				to execute the main image;
 *
 *  output - calculated address of application;
 *
 * Return:
 * fih_int
 *
 *******************************************************************************/
static inline __attribute__((always_inline)) fih_int fih_calc_app_addr(
    uintptr_t image_base, const struct boot_rsp *rsp, fih_uint *app_address)
{
    fih_int fih_rc = FIH_FAILURE;

#if defined(MCUBOOT_RAM_LOAD)
    if (IS_RAM_BOOTABLE(rsp->br_hdr) == true) {
        if ((UINT32_MAX - rsp->br_hdr->ih_hdr_size) >= image_base) {
            *app_address =
                fih_uint_encode(image_base + rsp->br_hdr->ih_hdr_size);
            fih_rc = FIH_SUCCESS;
        }
    } else
#endif
    {
        if (((UINT32_MAX - rsp->br_image_off) >= image_base) &&
            ((UINT32_MAX - rsp->br_hdr->ih_hdr_size) >=
             (image_base + rsp->br_image_off))) {
            *app_address = fih_uint_encode(image_base + rsp->br_image_off +
                                           rsp->br_hdr->ih_hdr_size);
            fih_rc = FIH_SUCCESS;
        }
    }

    FIH_RET(fih_rc);
}

#if defined CYW20829

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

#endif /* defined CYW20829 */

static bool do_boot(struct boot_rsp *rsp)
{
    uintptr_t flash_base = 0;

#if defined CYW20829
    uint32_t *key = NULL;
    uint32_t *iv = NULL;
#endif /* defined CYW20829 */

    if ((rsp != NULL) && (rsp->br_hdr != NULL)) {

        if (flash_device_base(rsp->br_flash_dev_id, &flash_base) == 0) {
            fih_uint app_addr = calc_app_addr(flash_base, rsp);

            BOOT_LOG_INF("Starting User Application (wait)...");
            if (IS_ENCRYPTED(rsp->br_hdr)) {
                BOOT_LOG_DBG(" * User application is encrypted");
            }
            BOOT_LOG_INF("Start slot Address: 0x%08" PRIx32, (uint32_t)fih_uint_decode(app_addr));

            if (flash_device_base(rsp->br_flash_dev_id, &flash_base) != 0) {
                return false;
            }

            if (!fih_uint_eq(calc_app_addr(flash_base, rsp), app_addr)) {
                return false;
            }

#if defined PSC3
    BOOT_LOG_INF("Launching app on CM33 core");
    BOOT_LOG_INF(BOOT_MSG_FINISH);
    hw_deinit();
    launch_cm33_app((void*)fih_uint_decode(app_addr));
#else

#if defined CYW20829

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
#ifdef BOOT_CM0P
            Cy_SysEnableCM4(fih_uint_decode(app_addr));
            return true;
#else
            psoc6_launch_cm4_app(app_addr);
#endif /* BOOT_CM0P */

#elif defined APP_CM0P
#ifdef BOOT_CM0P
            /* This function does not return */
            BOOT_LOG_INF("Launching app on CM0P core");
            BOOT_LOG_INF(BOOT_MSG_FINISH);
            hw_deinit();
            psoc6_launch_cm0p_app(app_addr);
#else
#error "Application should run on Cortex-M4"
#endif /* BOOT_CM0P */

#elif defined APP_CM7
            /* This function does not return */
            BOOT_LOG_INF("Launching app on CM7 core");
            BOOT_LOG_INF(BOOT_MSG_FINISH);
            hw_deinit();
            xmc7000_launch_cm7_app(app_addr);
            return true;
#else
#error "Application should run on either Cortex-M0+ or Cortex-M4"
#endif /* APP_CM4 */
#endif

#endif /* defined CYW20829 */
        } else {
            BOOT_LOG_ERR("Flash device ID not found");
            return false;
        }
    }

    return false;
}

static void DeepSleep_Prepare(void)
{
    static cy_stc_syspm_callback_params_t syspmSleepAppParams;
    static cy_stc_syspm_callback_t syspmAppSleepCallbackHandler =
        {
            Cy_SCB_UART_DeepSleepCallback, CY_SYSPM_DEEPSLEEP, 0u, &syspmSleepAppParams,
            NULL, NULL, 0};

    syspmSleepAppParams.base = cy_retarget_io_uart_obj.base;
    syspmSleepAppParams.context = (void *)&(cy_retarget_io_uart_obj.context);

    if (!Cy_SysPm_RegisterCallback(&syspmAppSleepCallbackHandler)) {
        BOOT_LOG_ERR("Failed to register syspmAppSleepCallbackHandler");
        CY_ASSERT(false);
    }
}

int main(void)
{
    struct boot_rsp rsp = {};
    bool boot_succeeded = false;
    fih_int fih_rc = FIH_FAILURE;
    cy_rslt_t rc = cybsp_init();


    if (rc != CY_RSLT_SUCCESS) {
        CY_ASSERT((bool)0);
        /* Loop forever... */
        while (true) {
            __WFI();
        }
    }

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
#if defined(CY_DEVICE_PSOC6ABLE2) && !defined(BOOT_CM4)
    if (CY_SYS_CM4_STATUS_ENABLED == Cy_SysGetCM4Status()) {
        Cy_SysDisableCM4();
    }
#endif /* defined(CY_DEVICE_PSOC6ABLE2) && !defined(BOOT_CM4) */
    /* Initialize retarget-io to use the debug UART port */
    rc = cy_retarget_io_init(CYBSP_DEBUG_UART_TX,
                             CYBSP_DEBUG_UART_RX,
                             CY_RETARGET_IO_BAUDRATE);

    if (rc != CY_RSLT_SUCCESS) {
        CY_ASSERT((bool)0);
        /* Loop forever... */
        while (true) {
            __WFI();
        }
    }

    #ifdef FIH_ENABLE_DELAY
    /*If random delay is used in FIH APIs then
     * fih_delay must be initialized */
    fih_delay_init();
    #endif /* FIH_ENABLE_DELAY */

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
        if (fih_eq(fih_rc, FIH_SUCCESS)) {
            BOOT_LOG_INF("User Application validated successfully");
            /* initialize watchdog timer. it should be updated from user app
            * to mark successful start up of this app. if the watchdog is not updated,
            * reset will be initiated by watchdog timer and swap revert operation started
            * to roll back to operable image.
            */
#if defined(USE_WDT_PDL)
            rc = initialize_wdt();
#else
            cyhal_wdt_t wdt_obj;

            rc = cyhal_wdt_init(&wdt_obj, WDT_TIME_OUT_MS);

            cyhal_wdt_start(&wdt_obj);
#endif

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

    DeepSleep_Prepare();

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
#if defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) && !defined(USE_XIP)
    qspi_deinit(SMIF_ID);
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) && !defined(MCUBOOT_ENC_IMAGES_XIP) */

    /* Flush the TX buffer, need to be fixed in retarget_io */
    while(cy_retarget_io_is_tx_active()){}
    cy_retarget_io_deinit();

#ifdef USE_EXEC_TIME_CHECK
    timebase_us_deinit();
#endif /* USE_EXEC_TIME_CHECK */

#ifdef USE_LOG_TIMESTAMP
    log_timestamp_deinit();
#endif /* USE_LOG_TIMESTAMP */
}
