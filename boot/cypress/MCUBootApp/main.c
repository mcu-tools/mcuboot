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

/* Define pins for UART debug output */
#define CYBSP_UART_ENABLED 1U
#define CYBSP_UART_HW SCB5
#define CYBSP_UART_IRQ scb_5_interrupt_IRQn

static void do_boot(struct boot_rsp *rsp)
{
    uint32_t app_addr = 0;

    app_addr = (rsp->br_image_off + rsp->br_hdr->ih_hdr_size);

    BOOT_LOG_INF("Starting User Application on CM4 (wait)...");
    BOOT_LOG_INF("Start Address: 0x%08x", app_addr);
    Cy_SysLib_Delay(100);

    Cy_SysEnableCM4(app_addr);

    while (1)
    {
        __WFI() ;
    }
}

int main(void)
{
    struct boot_rsp rsp;

    init_cycfg_clocks();
    init_cycfg_peripherals();
    init_cycfg_pins();
    /* enable interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port (CYBSP_UART_HW) */
    cy_retarget_io_pdl_init(115200u);

    BOOT_LOG_INF("MCUBoot Bootloader Started");

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    cy_rslt_t rc = !CY_RSLT_SUCCESS;

    #undef MCUBOOT_MAX_IMG_SECTORS
    /* redefine number of sectors as there 2MB will be
     * available on PSoC062-2M in case of external
     * memory usage */
    #define MCUBOOT_MAX_IMG_SECTORS 4096
    int smif_id = 1; /* Assume SlaveSelect_0 is used for External Memory */
    /* Acceptable values are:
    * 0 - SMIF disabled (no external memory);
    * 1, 2, 3 or 4 - slave select line memory module is connected to.
    */
    rc = qspi_init_sfdp(smif_id);
    if(rc == CY_SMIF_SUCCESS)
    {
        BOOT_LOG_INF("External Memory initialized w/ SFDP.");
    }
    else
    {
        BOOT_LOG_ERR("External Memory initialization w/ SFDP FAILED: 0x%02x", (int)rc);
    }
    if(0 == rc)
#endif
    {
        if (boot_go(&rsp) == 0) {
            BOOT_LOG_INF("User Application validated successfully");
            do_boot(&rsp);
        } else
            BOOT_LOG_INF("MCUBoot Bootloader found none of bootable images") ;
    }
    return 0;
}
