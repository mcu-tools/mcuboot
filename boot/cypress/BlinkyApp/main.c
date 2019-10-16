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
* \copyright
* Copyright 2017-2019 Cypress Semiconductor Corporation
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

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

#ifdef BOOT_IMG
    #define BLINK_PERIOD          (1000u)
    #define GREETING_MESSAGE_VER  "[BlinkyApp] BlinkyApp v1.0\n"
    #define GREETING_MESSAGE_INFO "[BlinkyApp] Red led blinks with 1 sec period\n"
#elif UPGRADE_IMG
    #define BLINK_PERIOD          (100u)
    #define GREETING_MESSAGE_VER  "[BlinkyApp] BlinkyApp v2.0 [+]\n"
    #define GREETING_MESSAGE_INFO "[BlinkyApp] Red led blinks with 0.5 sec period\n"
#else
    #error "[BlinkyApp] Please specify type of image: -DBOOT_IMG or -DUPGRADE_IMG\n\r"
#endif

void check_result(int res)
{
    if (res != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
}

void test_app_init_hardware(void)
{
    int rc = !CY_RSLT_SUCCESS;

    cybsp_init();

    /* enable interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    check_result(cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                                     CY_RETARGET_IO_BAUDRATE));
    
    printf("===========================\n");
    printf(GREETING_MESSAGE_VER);
    printf("===========================\n");

    /* Initialize the User LED */
    check_result(cyhal_gpio_init((cyhal_gpio_t) CYBSP_USER_LED1, CYHAL_GPIO_DIR_OUTPUT,
                                 CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF));

    printf("\r[BlinkyApp] GPIO initialized \n");
    printf("[BlinkyApp] UART initialized \n");
    printf("[BlinkyApp] Retarget I/O set to 115200 baudrate \n");
    
}

int main(void)
{
    uint32_t blinky_period = BLINK_PERIOD;

    test_app_init_hardware();

    printf(GREETING_MESSAGE_INFO);

    for (;;)
    {
		/* Toggle the user LED periodically */
        Cy_SysLib_Delay(blinky_period);

        /* Invert the USER LED state */
        cyhal_gpio_toggle((cyhal_gpio_t) CYBSP_USER_LED1);
    }
}
