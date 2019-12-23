/***************************************************************************//**
* \file main.c
* \version 1.0
*
* \brief
* Demonstrates blinking an LED under firmware control. The Cortex-M4 toggles
* the Red LED. The Cortex-M0+ starts the Cortex-M4 and enters sleep.
*
* Compatible Kits:
*   CY8CKIT-062-BLE
*   CY8CKIT-062-WIFI-BT
*
* Migration to CY8CPROTO-062-4343W kit (ModusToolbox IDE):
*   1. Create this project targeting the CY8CPROTO-062-4343W kit.
*   2. Open design.modus file and replicate P0[3] configuration on P13[7]. Give
*      this pin the alias "LED_RED". Disable P0[3].
*   3. Build and Program
*
* Migration to CY8CPROTO-062-4343W kit (command line make):
*   1. Launch the Device Configurator tool from
*      ModusToolbox_1.0\tools\device-configurator-1.0\
*   2. In Device Configurator, open design.modus file
*      (ModusToolbox_1.0\libraries\psoc6sw-1.0\examples\BlinkyLED\design.modus)
*      and replicate P0[3] configuration on P13[7].
*      Give this pin the alias "LED_RED". Disable P0[3].
*   3. Perform "make clean"
*   4. Build and Program the device with "make DEVICE=CY8C624ABZI-D44 program"
*      Note that depending on the method used to program the device, you may
*      need to manually reset it by pressing the SW1 RESET button on the kit.
*   4. Observe the red blinking LED.
*   5. To switch back to CY8CKIT-062-BLE or CY8CKIT-062-WIFI-BT,
*      perform steps 1 through 3 to reconfigure the "LED_RED" to P0[3].
*      Then use "make program".
*
********************************************************************************
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

/* Cypress pdl/bsp headers */
#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cy_result.h"

#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"

#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/sign_key.h"

#include "bootutil/bootutil_log.h"

static void do_boot(struct boot_rsp *rsp)
{
    uint32_t app_addr = 0;

    app_addr = (rsp->br_image_off + rsp->br_hdr->ih_hdr_size);

    BOOT_LOG_INF("Starting User Application on CM4 (wait)...");
    Cy_SysLib_Delay(100);

    cy_retarget_io_deinit();

    Cy_SysEnableCM4(app_addr);

    while (1)
    {
        __WFI() ;
    }
}

int main(void)
{
    cy_rslt_t rc = !CY_RSLT_SUCCESS;
    struct boot_rsp rsp ;

    /* Initialize the device and board peripherals */
    rc = cybsp_init();

    if (rc != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* enable interrupts */
    if (rc != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    else
    {
        BOOT_LOG_INF("MCUBoot Bootloader Started");
    }
    __enable_irq();

    if (boot_go(&rsp) == 0)
    {
        BOOT_LOG_INF("User Application validated successfully");
        do_boot(&rsp);
    }
    else
        BOOT_LOG_INF("MCUBoot Bootloader found none of bootable images") ;

    return 0;
}
