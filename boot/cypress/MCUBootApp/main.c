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
/* Cypress pdl headers */
#include "cy_pdl.h"
#include "cy_retarget_io_pdl.h"
#include "cy_result.h"

#include "cycfg_clocks.h"
#include "cycfg_peripherals.h"
#include "cycfg_pins.h"

#include "flash_qspi.h"
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

/* Define pins for UART debug output */
#define CYBSP_UART_ENABLED 1U
#define CYBSP_UART_HW SCB5
#define CYBSP_UART_IRQ scb_5_interrupt_IRQn

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
/* Choose SMIF slot number (slave select).
 * Acceptable values are:
 * 0 - SMIF disabled (no external memory);
 * 1, 2, 3 or 4 - slave select line memory module is connected to.
 */
uint32_t smif_id = 1; /* Assume SlaveSelect_0 is used for External Memory */
#endif


void hw_deinit(void);

static void do_boot(struct boot_rsp *rsp)
{
    uint32_t app_addr = 0;

    app_addr = (rsp->br_image_off + rsp->br_hdr->ih_hdr_size);

    BOOT_LOG_INF("Starting User Application on CM4 (wait)...");
    BOOT_LOG_INF("Start Address: 0x%08lx", app_addr);
    BOOT_LOG_INF("Deinitializing hardware...");

    cy_retarget_io_wait_tx_complete(CYBSP_UART_HW, 10);

    hw_deinit();

    Cy_SysEnableCM4(app_addr);
}

int main(void)
{
    struct boot_rsp rsp;
    cy_rslt_t rc = CY_RSLT_TYPE_ERROR;
    bool boot_succeeded = false;
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    SystemInit();
    //init_cycfg_clocks();
    init_cycfg_peripherals();
    init_cycfg_pins();

    /* Certain PSoC 6 devices enable CM4 by default at startup. It must be 
     * either disabled or enabled & running a valid application for flash write
     * to work from CM0+. Since flash write may happen in boot_go() for updating
     * the image before this bootloader app can enable CM4 in do_boot(), we need
     * to keep CM4 disabled. Note that debugging of CM4 is not supported when it
     * is disabled.
     */
    #if defined(CY_DEVICE_PSOC6ABLE2)
    if (CY_SYS_CM4_STATUS_ENABLED == Cy_SysGetCM4Status())
    {
        Cy_SysDisableCM4();
    }
    #endif /* #if defined(CY_DEVICE_PSOC6ABLE2) */

    /* enable interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port (CYBSP_UART_HW) */
    rc = cy_retarget_io_pdl_init(115200u);

    if (rc != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    BOOT_LOG_INF("MCUBoot Bootloader Started");

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    rc = CY_SMIF_CMD_NOT_FOUND;

    #undef MCUBOOT_MAX_IMG_SECTORS
    /* redefine number of sectors as there 2MB will be
     * available on PSoC062-2M in case of external
     * memory usage */
    #define MCUBOOT_MAX_IMG_SECTORS 4096
    rc = qspi_init_sfdp(smif_id);
    if (rc == CY_SMIF_SUCCESS)
    {
        BOOT_LOG_INF("External Memory initialized w/ SFDP.");
    }
    else
    {
        BOOT_LOG_ERR("External Memory initialization w/ SFDP FAILED: 0x%02x", (int)rc);
    }
    if (CY_SMIF_SUCCESS == rc)
#endif
    {

        FIH_CALL(boot_go, fih_rc, &rsp);
        if (FIH_EQ(fih_rc, FIH_SUCCESS))
        {
            BOOT_LOG_INF("User Application validated successfully");
            /* initialize watchdog timer. it should be updated from user app
            * to mark successful start up of this app. if the watchdog is not updated,
            * reset will be initiated by watchdog timer and swap revert operation started
            * to roll back to operable image.
            */
            cy_wdg_init(WDT_TIME_OUT_MS);
            do_boot(&rsp);
            boot_succeeded = true;
        }
        else
        {
            BOOT_LOG_INF("MCUBoot Bootloader found none of bootable images");
        }
    }

    while (1)
    {
        if (boot_succeeded) {
            Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
        }
        else {
            __WFI();
        }
    }

    return 0;
}

void hw_deinit(void)
{
    cy_retarget_io_pdl_deinit();
    Cy_GPIO_Port_Deinit(CYBSP_UART_RX_PORT);
    Cy_GPIO_Port_Deinit(CYBSP_UART_TX_PORT);

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    qspi_deinit(smif_id);
#endif
}
