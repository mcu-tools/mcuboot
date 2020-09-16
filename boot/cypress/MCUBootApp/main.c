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

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
/* Choose SMIF slot number (slave select).
 * Acceptable values are:
 * 0 - SMIF disabled (no external memory);
 * 1, 2, 3 or 4 - slave select line memory module is connected to.
 */
uint32_t smif_id = 1; /* Assume SlaveSelect_0 is used for External Memory */
#endif

extern cy_stc_scb_uart_context_t CYBSP_UART_context;

/* Parameter structures for callback function */
static cy_stc_syspm_callback_params_t deep_sleep_clbk_params = 
{
    CYBSP_UART_HW,
    &CYBSP_UART_context
};

static cy_stc_syspm_callback_params_t deep_sleep_sysclk_pm_clbk_param =
{
    NULL,
    NULL
};

/* Callback structure */
cy_stc_syspm_callback_t uart_deep_sleep = 
{
    &Cy_SCB_UART_DeepSleepCallback, 
    CY_SYSPM_DEEPSLEEP,
    CY_SYSPM_SKIP_BEFORE_TRANSITION ,
    &deep_sleep_clbk_params,
    NULL,
    NULL,
    0
};

cy_stc_syspm_callback_t clk_deep_sleep = 
{
    &Cy_SysClk_DeepSleepCallback, 
    CY_SYSPM_DEEPSLEEP,
    CY_SYSPM_SKIP_BEFORE_TRANSITION ,
    &deep_sleep_sysclk_pm_clbk_param,
    NULL,
    NULL,
    0
};

void hw_deinit(void);

static void do_boot(struct boot_rsp *rsp)
{
    uint32_t app_addr = 0;

    app_addr = (rsp->br_image_off + rsp->br_hdr->ih_hdr_size);

    BOOT_LOG_INF("Starting User Application on CM4 (wait)...");
    BOOT_LOG_INF("Start Address: 0x%08lx", app_addr);
    Cy_SysLib_Delay(100);

    hw_deinit();

    Cy_SysEnableCM4(app_addr);
}

int main(void)
{
    struct boot_rsp rsp;
    cy_rslt_t result = CY_RSLT_TYPE_ERROR;

    init_cycfg_clocks();
    init_cycfg_peripherals();
    init_cycfg_pins();

    /* register callback funtions that manage peripherals before going to deep sleep */
    if (!Cy_SysPm_RegisterCallback(&uart_deep_sleep) || 
        !Cy_SysPm_RegisterCallback(&clk_deep_sleep))
    {
        CY_ASSERT(0);
    }
    /* enable interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port (CYBSP_UART_HW) */
    result = cy_retarget_io_pdl_init(115200u);

    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    BOOT_LOG_INF("MCUBoot Bootloader Started");

#ifdef CY_BOOT_USE_EXTERNAL_FLASH
    cy_rslt_t rc = !CY_RSLT_SUCCESS;

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
    if (0 == rc)
#endif
    {
        if (boot_go(&rsp) == 0)
        {
            BOOT_LOG_INF("User Application validated successfully");
            do_boot(&rsp);
        }
        else
        {
            BOOT_LOG_INF("MCUBoot Bootloader found none of bootable images");
        }
    }

    while (1)
    {
        Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
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
