#ifndef PLATFORM_H
#define PLATFORM_H

#include <inttypes.h>
#include <stdio.h>

#include "cy_pdl.h"
#include "cy_retarget_io.h"
#include "cybsp.h"
#include "cycfg.h"
#include "cyhal.h"
#include "cyhal_wdt.h"
#include "cy_wdt.h"

#if defined(CY_BOOT_USE_EXTERNAL_FLASH) || defined(CYW20829)
#include "flash_qspi.h"
#endif /* defined(CY_BOOT_USE_EXTERNAL_FLASH) || defined(CYW20829) */

#ifdef BOOT_IMAGE
#define IMAGE_TYPE "BOOT"
#define BLINK_PERIOD (1000u)
#define GREETING_MESSAGE_INFO "[BlinkyApp] Red led blinks with 1 sec period\r\n"
#elif defined(UPGRADE_IMAGE)
#define IMAGE_TYPE "UPGRADE"
#define BLINK_PERIOD (250u)
#define GREETING_MESSAGE_INFO "[BlinkyApp] Red led blinks with 0.25 sec period\r\n"
#else
#error "[BlinkyApp] Please specify type of image: -DBOOT_IMAGE or -DUPGRADE_IMAGE\r\n"
#endif /* BOOT_IMAGE */

#define GREETING_MESSAGE_VER "[BlinkyApp] Version:"

#define WATCHDOG_FREE_MESSAGE "[BlinkyApp] Turn off watchdog timer\r\n"

#define SMIF_ID (1U) /* Assume SlaveSelect_0 is used for External Memory */

static const char* core33_message = "CM33";
static const char* core0p_message = "CM0P";
static const char* core4_message = "CM4";
static const char* core7_message = "CM7";

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

static inline const char* test_app_init_hardware(void)
{
    const char* detect_core_message = NULL;
    (void)core33_message;
    (void)core0p_message;
    (void)core4_message;
    (void)core7_message;
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

    cybsp_init();
    /* enable interrupts */
    __enable_irq();

    /* Initialize led port */
    Cy_GPIO_Pin_Init(LED_PORT, LED_PIN, &LED_config);

    res = cy_retarget_io_init(CY_DEBUG_UART_TX, CY_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

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

    rc = qspi_init_sfdp(SMIF_ID);
    if (CY_SMIF_SUCCESS == rc) {
        printf("[BlinkyApp] External Memory initialized w/ SFDP. \r\n");
    } else {
        printf("[BlinkyApp] External Memory initialization w/ SFDP FAILED: 0x%" PRIx32 " \r\n", (uint32_t)rc);
    }

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
    } else if (CPUSS_MS_ID_CM0 == core) {
        detect_core_message = core0p_message;
    } else
#endif /* APP_CM0P */
    {
        detect_core_message = core4_message;
    }
#ifdef APP_CM7
    if (CPUSS_MS_ID_CM7_0 == _FLD2VAL(CPUSS_IDENTITY_MS, CPUSS->IDENTITY)) {
        detect_core_message = core7_message;
    }
#endif

    printf("===========================\r\n");
#endif /* CYW20829 */
    printf("[BlinkyApp] GPIO initialized \r\n");
    printf("[BlinkyApp] UART initialized \r\n");
    printf("[BlinkyApp] Retarget I/O set to 115200 baudrate \r\n");
#if defined(USE_WDT_PDL)
    Cy_WDT_ClearWatchdog();
#else
    cyhal_wdt_kick(NULL);
#endif

    return (detect_core_message);
}

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* PLATFORM_H */
