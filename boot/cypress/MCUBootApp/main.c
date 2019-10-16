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

/* Cypress pdl/bsp headers */
#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

#include "sysflash/sysflash.h"
#include "flash_map_backend/flash_map_backend.h"

#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/sign_key.h"

unsigned char ecdsa_pub_key[] = {
  0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
  0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
  0x42, 0x00, 0x04, 0x15, 0x96, 0xcf, 0x02, 0xec, 0x76, 0xb5, 0x27, 0x35,
  0xb8, 0x7e, 0x96, 0xee, 0x67, 0xf2, 0x63, 0x2b, 0x38, 0x00, 0xb8, 0x34,
  0x4e, 0x2e, 0x06, 0xb5, 0x75, 0x8e, 0xc2, 0xc9, 0xfa, 0x31, 0x81, 0x87,
  0x06, 0x39, 0x7a, 0xc4, 0x21, 0x95, 0xe4, 0x77, 0xb9, 0xf2, 0xa1, 0x41,
  0x63, 0x40, 0x05, 0x55, 0xdc, 0x37, 0x67, 0xe0, 0x5e, 0xef, 0xfa, 0xfe,
  0xa2, 0x64, 0xc2, 0x09, 0x49, 0x34, 0x87
};

unsigned int ecdsa_pub_key_len = 91;

const struct bootutil_key bootutil_keys[] = {
    {
#if defined(MCUBOOT_SIGN_RSA)
        .key = rsa_pub_key,
        .len = &rsa_pub_key_len,
#elif defined(MCUBOOT_SIGN_EC256)
        .key = ecdsa_pub_key,
        .len = &ecdsa_pub_key_len,
#elif defined(MCUBOOT_SIGN_ED25519)
        .key = ed25519_pub_key,
        .len = &ed25519_pub_key_len,
#endif
    },
};
const int bootutil_key_cnt = 1;

static void do_boot(struct boot_rsp *rsp)
{
    printf("Starting Application (wait) ...\r\n") ;
    Cy_SysEnableCM4(rsp->br_hdr->ih_load_addr) ;
    while (1)
    {
        __WFI() ;
    }
}

int main(void)
{
    struct boot_rsp rsp ;

    /* Initialize the device and board peripherals */
    int result = cybsp_init();

    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* enable interrupts */
    __enable_irq();

    printf("MCUBoot Application Started\n\r");

    if (boot_go(&rsp) == 0)
        do_boot(&rsp) ;
    else
        printf("MCUBoot no bootable image\n") ;

    return 0;
}
