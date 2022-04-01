#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef CYW20829
#include <inttypes.h>

#include "cybsp.h"
#include "cycfg_pins.h"
#include "cyhal_wdt.h"
#else
#include "system_psoc6.h"
#endif /* CYW20829 */

#include <stdio.h>

#include "cy_pdl.h"
#ifdef APP_CM0P
#include "cycfg_peripherals.h"
#include "cycfg_pins.h"
#include "cy_retarget_io_pdl.h"
#else
#include "cyhal.h"
#include "cy_retarget_io.h"

#endif /* APP_CM0P */
#include "watchdog.h"

#if defined(CY_BOOT_USE_EXTERNAL_FLASH) || defined(CYW20829)
#include "flash_qspi.h"
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) || defined(CYW20829) */

#ifdef BOOT_IMAGE
#define IMAGE_TYPE             "BOOT"
#define BLINK_PERIOD           (1000u)
#define GREETING_MESSAGE_INFO  "[BlinkyApp] Red led blinks with 1 sec period\r\n"
#elif defined(UPGRADE_IMAGE)
#define IMAGE_TYPE             "UPGRADE"
#define BLINK_PERIOD           (250u)
#define GREETING_MESSAGE_INFO  "[BlinkyApp] Red led blinks with 0.25 sec period\r\n"
#else
#error                         "[BlinkyApp] Please specify type of image: -DBOOT_IMAGE or -DUPGRADE_IMAGE\r\n"
#endif /* BOOT_IMAGE */

#define GREETING_MESSAGE_VER   "[BlinkyApp] Version:"

#define WATCHDOG_FREE_MESSAGE  "[BlinkyApp] Turn off watchdog timer\r\n"

#define SMIF_ID (1U) /* Assume SlaveSelect_0 is used for External Memory */

static const char* core33_message ="CM33";
static const char* core0p_message ="CM0P";
static const char* core4_message  ="CM4";

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

static inline const char* test_app_init_hardware(void)
{
    const char* detect_core_message = NULL;
    (void) core33_message;
    (void) core0p_message;
    (void) core4_message;
    cy_rslt_t res = CY_RSLT_TYPE_ERROR;

    const cy_stc_gpio_pin_config_t LED_config = {
        .outVal = 1,
        .driveMode = CY_GPIO_DM_STRONG_IN_OFF,
        .hsiom = HSIOM_SEL_GPIO,
        .intEdge = CY_GPIO_INTR_DISABLE,
        .intMask = 0UL,
        .vtrip = CY_GPIO_VTRIP_CMOS,
        .slewRate = CY_GPIO_SLEW_FAST,
        .driveSel = CY_GPIO_DRIVE_FULL,
        .vregEn = 0UL,
        .ibufMode = 0UL,
        .vtripSel = 0UL,
        .vrefSel = 0UL,
        .vohSel = 0UL,
    };

#ifdef CYW20829
    cybsp_init();
#elif defined APP_CM0P
    init_cycfg_peripherals();
    init_cycfg_pins();
#endif /* CYW20829 */

    /* enable interrupts */
    __enable_irq();

    /* Initialize led port */
    Cy_GPIO_Pin_Init(LED_PORT, LED_PIN, &LED_config);

    /* Initialize retarget-io to use the debug UART port */
#ifdef APP_CM0P
    res = cy_retarget_io_pdl_init(CY_RETARGET_IO_BAUDRATE);
#else
    res = cy_retarget_io_init(CY_DEBUG_UART_TX, CY_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
#endif /* APP_CM0P */

    if (res != CY_RSLT_SUCCESS) {
        CY_ASSERT(0);
        /* Loop forever... */
        for (;;) {
        }
    }

    printf("\n===========================\r\n");
    printf("%s %s\r\n", GREETING_MESSAGE_VER, IMG_VER_MSG);

#ifdef CYW20829
    detect_core_message = core33_message;

    printf("===========================\r\n");

    cy_en_smif_status_t rc = CY_SMIF_CMD_NOT_FOUND;
    cyhal_wdt_t *cyw20829_wdt = NULL;

    rc = qspi_init_sfdp(SMIF_ID);
    if (CY_SMIF_SUCCESS == rc) {
        printf("[BlinkyApp] External Memory initialized w/ SFDP. \r\n");
    } else {
        printf("[BlinkyApp] External Memory initialization w/ SFDP FAILED: 0x%" PRIx32 " \r\n", (uint32_t)rc);
    }

    /* Disable watchdog timer to mark successful start up of application. */
    cyhal_wdt_free(cyw20829_wdt);

#else
    /* Determine on which core this app is running by polling CPUSS_IDENTITY register.
     * This register contains bits field [8:11]. This field specifies the bus master
     * identifier of the transfer that reads the register.
     */
#ifdef APP_CM0P

    en_prot_master_t core = _FLD2VAL(CPUSS_IDENTITY_MS, CPUSS->IDENTITY);

    if (CPUSS_MS_ID_CM4 == core) {
        printf("\n[BlinkyApp] is compiled for CM0P core, started on CM4 instead. Execution Halted.\n");
        CY_ASSERT(0);
    }
    else if (CPUSS_MS_ID_CM0 == core) {
        detect_core_message = core0p_message;
    }
    else
#endif /* APP_CM0P */
    {
        detect_core_message = core4_message;
    }
    printf("===========================\r\n");
    cy_wdg_free();
#endif /* CYW20829 */
    printf("[BlinkyApp] GPIO initialized \r\n");
    printf("[BlinkyApp] UART initialized \r\n");
    printf("[BlinkyApp] Retarget I/O set to 115200 baudrate \r\n");
    printf(WATCHDOG_FREE_MESSAGE);

    return(detect_core_message);
}

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* PLATFORM_H */
