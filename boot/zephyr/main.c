/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Arm Limited
 * Copyright (c) 2021 Nordic Semiconductor ASA
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
 */

#include <assert.h>
#include <zephyr.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <sys/__assert.h>
#include <drivers/flash.h>
#include <drivers/timer/system_timer.h>
#include <usb/usb_device.h>
#include <soc.h>
#include <linker/linker-defs.h>

#include "target.h"

#include "bootutil/bootutil_log.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/mcuboot_status.h"
#include "flash_map_backend/flash_map_backend.h"

#ifdef CONFIG_MCUBOOT_SERIAL
#include "boot_serial/boot_serial.h"
#include "serial_adapter/serial_adapter.h"

const struct boot_uart_funcs boot_funcs = {
    .read = console_read,
    .write = console_write
};
#endif

#if defined(CONFIG_BOOT_USB_DFU_WAIT) || defined(CONFIG_BOOT_USB_DFU_GPIO)
#include <usb/class/usb_dfu.h>
#endif

#if CONFIG_MCUBOOT_CLEANUP_ARM_CORE
#include <arm_cleanup.h>
#endif

/* CONFIG_LOG_MINIMAL is the legacy Kconfig property,
 * replaced by CONFIG_LOG_MODE_MINIMAL.
 */
#if (defined(CONFIG_LOG_MODE_MINIMAL) || defined(CONFIG_LOG_MINIMAL))
#define ZEPHYR_LOG_MODE_MINIMAL 1
#endif

/* CONFIG_LOG_IMMEDIATE is the legacy Kconfig property,
 * replaced by CONFIG_LOG_MODE_IMMEDIATE.
 */
#if (defined(CONFIG_LOG_MODE_IMMEDIATE) || defined(CONFIG_LOG_IMMEDIATE))
#define ZEPHYR_LOG_MODE_IMMEDIATE 1
#endif

#if defined(CONFIG_LOG) && !defined(ZEPHYR_LOG_MODE_IMMEDIATE) && \
    !defined(ZEPHYR_LOG_MODE_MINIMAL)
#ifdef CONFIG_LOG_PROCESS_THREAD
#warning "The log internal thread for log processing can't transfer the log"\
         "well for MCUBoot."
#else
#include <logging/log_ctrl.h>

#define BOOT_LOG_PROCESSING_INTERVAL K_MSEC(30) /* [ms] */

/* log are processing in custom routine */
K_THREAD_STACK_DEFINE(boot_log_stack, CONFIG_MCUBOOT_LOG_THREAD_STACK_SIZE);
struct k_thread boot_log_thread;
volatile bool boot_log_stop = false;
K_SEM_DEFINE(boot_log_sem, 1, 1);

/* log processing need to be initalized by the application */
#define ZEPHYR_BOOT_LOG_START() zephyr_boot_log_start()
#define ZEPHYR_BOOT_LOG_STOP() zephyr_boot_log_stop()
#endif /* CONFIG_LOG_PROCESS_THREAD */
#else
/* synchronous log mode doesn't need to be initalized by the application */
#define ZEPHYR_BOOT_LOG_START() do { } while (false)
#define ZEPHYR_BOOT_LOG_STOP() do { } while (false)
#endif /* defined(CONFIG_LOG) && !defined(ZEPHYR_LOG_MODE_IMMEDIATE) && \
        * !defined(ZEPHYR_LOG_MODE_MINIMAL)
	*/

#ifdef CONFIG_SOC_FAMILY_NRF
#include <helpers/nrfx_reset_reason.h>

static inline bool boot_skip_serial_recovery()
{
    uint32_t rr = nrfx_reset_reason_get();

    return !(rr == 0 || (rr & NRFX_RESET_REASON_RESETPIN_MASK));
}
#else
static inline bool boot_skip_serial_recovery()
{
    return false;
}
#endif

BOOT_LOG_MODULE_REGISTER(mcuboot);

#ifdef CONFIG_MCUBOOT_INDICATION_LED
/*
 * Devicetree helper macro which gets the 'flags' cell from a 'gpios'
 * property, or returns 0 if the property has no 'flags' cell.
 */
#define FLAGS_OR_ZERO(node)                        \
  COND_CODE_1(DT_PHA_HAS_CELL(node, gpios, flags), \
              (DT_GPIO_FLAGS(node, gpios)),        \
              (0))

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */

#define LED0_NODE DT_ALIAS(bootloader_led0)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay) && DT_NODE_HAS_PROP(LED0_NODE, gpios)
#define LED0_GPIO_LABEL DT_GPIO_LABEL(LED0_NODE, gpios)
#define LED0_GPIO_PIN DT_GPIO_PIN(LED0_NODE, gpios)
#define LED0_GPIO_FLAGS (GPIO_OUTPUT | FLAGS_OR_ZERO(LED0_NODE))
#else
/* A build error here means your board isn't set up to drive an LED. */
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

const static struct device *led;

void led_init(void)
{

  led = device_get_binding(LED0_GPIO_LABEL);
  if (led == NULL) {
    BOOT_LOG_ERR("Didn't find LED device %s\n", LED0_GPIO_LABEL);
    return;
  }

  gpio_pin_configure(led, LED0_GPIO_PIN, LED0_GPIO_FLAGS);
  gpio_pin_set(led, LED0_GPIO_PIN, 0);

}
#endif

void os_heap_init(void);

#if defined(CONFIG_ARM)

#ifdef CONFIG_SW_VECTOR_RELAY
extern void *_vector_table_pointer;
#endif

struct arm_vector_table {
    uint32_t msp;
    uint32_t reset;
};

static void do_boot(struct boot_rsp *rsp)
{
    struct arm_vector_table *vt;
    uintptr_t flash_base;
    int rc;

    /* The beginning of the image is the ARM vector table, containing
     * the initial stack pointer address and the reset vector
     * consecutively. Manually set the stack pointer and jump into the
     * reset vector
     */
    rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
    assert(rc == 0);

    vt = (struct arm_vector_table *)(flash_base +
                                     rsp->br_image_off +
                                     rsp->br_hdr->ih_hdr_size);

    sys_clock_disable();

#ifdef CONFIG_USB_DEVICE_STACK
    /* Disable the USB to prevent it from firing interrupts */
    usb_disable();
#endif
#if CONFIG_MCUBOOT_CLEANUP_ARM_CORE
    cleanup_arm_nvic(); /* cleanup NVIC registers */

#ifdef CONFIG_CPU_CORTEX_M7
    /* Disable instruction cache and data cache before chain-load the application */
    SCB_DisableDCache();
    SCB_DisableICache();
#endif

#if CONFIG_CPU_HAS_ARM_MPU || CONFIG_CPU_HAS_NXP_MPU
    z_arm_clear_arm_mpu_config();
#endif

#if defined(CONFIG_BUILTIN_STACK_GUARD) && \
    defined(CONFIG_CPU_CORTEX_M_HAS_SPLIM)
    /* Reset limit registers to avoid inflicting stack overflow on image
     * being booted.
     */
    __set_PSPLIM(0);
    __set_MSPLIM(0);
#endif

#else
    irq_lock();
#endif /* CONFIG_MCUBOOT_CLEANUP_ARM_CORE */

#ifdef CONFIG_BOOT_INTR_VEC_RELOC
#if defined(CONFIG_SW_VECTOR_RELAY)
    _vector_table_pointer = vt;
#ifdef CONFIG_CPU_CORTEX_M_HAS_VTOR
    SCB->VTOR = (uint32_t)__vector_relay_table;
#endif
#elif defined(CONFIG_CPU_CORTEX_M_HAS_VTOR)
    SCB->VTOR = (uint32_t)vt;
#endif /* CONFIG_SW_VECTOR_RELAY */
#else /* CONFIG_BOOT_INTR_VEC_RELOC */
#if defined(CONFIG_CPU_CORTEX_M_HAS_VTOR) && defined(CONFIG_SW_VECTOR_RELAY)
    _vector_table_pointer = _vector_start;
    SCB->VTOR = (uint32_t)__vector_relay_table;
#endif
#endif /* CONFIG_BOOT_INTR_VEC_RELOC */

    __set_MSP(vt->msp);
#if CONFIG_MCUBOOT_CLEANUP_ARM_CORE
    __set_CONTROL(0x00); /* application will configures core on its own */
    __ISB();
#endif
    ((void (*)(void))vt->reset)();
}

#elif defined(CONFIG_XTENSA)
#define SRAM_BASE_ADDRESS	0xBE030000

static void copy_img_to_SRAM(int slot, unsigned int hdr_offset)
{
    const struct flash_area *fap;
    int area_id;
    int rc;
    unsigned char *dst = (unsigned char *)(SRAM_BASE_ADDRESS + hdr_offset);

    BOOT_LOG_INF("Copying image to SRAM");

    area_id = flash_area_id_from_image_slot(slot);
    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        BOOT_LOG_ERR("flash_area_open failed with %d\n", rc);
        goto done;
    }

    rc = flash_area_read(fap, hdr_offset, dst, fap->fa_size - hdr_offset);
    if (rc != 0) {
        BOOT_LOG_ERR("flash_area_read failed with %d\n", rc);
        goto done;
    }

done:
    flash_area_close(fap);
}

/* Entry point (.ResetVector) is at the very beginning of the image.
 * Simply copy the image to a suitable location and jump there.
 */
static void do_boot(struct boot_rsp *rsp)
{
    void *start;

    BOOT_LOG_INF("br_image_off = 0x%x\n", rsp->br_image_off);
    BOOT_LOG_INF("ih_hdr_size = 0x%x\n", rsp->br_hdr->ih_hdr_size);

    /* Copy from the flash to HP SRAM */
    copy_img_to_SRAM(0, rsp->br_hdr->ih_hdr_size);

    /* Jump to entry point */
    start = (void *)(SRAM_BASE_ADDRESS + rsp->br_hdr->ih_hdr_size);
    ((void (*)(void))start)();
}

#else
/* Default: Assume entry point is at the very beginning of the image. Simply
 * lock interrupts and jump there. This is the right thing to do for X86 and
 * possibly other platforms.
 */
static void do_boot(struct boot_rsp *rsp)
{
    void *start;
    
#if defined(MCUBOOT_RAM_LOAD)
    start = (void *)(rsp->br_hdr->ih_load_addr + rsp->br_hdr->ih_hdr_size);
#else
    uintptr_t flash_base;
    int rc;

    rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
    assert(rc == 0);

    start = (void *)(flash_base + rsp->br_image_off +
                     rsp->br_hdr->ih_hdr_size);
#endif

    /* Lock interrupts and dive into the entry point */
    irq_lock();
    ((void (*)(void))start)();
}
#endif

#if defined(CONFIG_LOG) && !defined(ZEPHYR_LOG_MODE_IMMEDIATE) && \
    !defined(CONFIG_LOG_PROCESS_THREAD) && !defined(ZEPHYR_LOG_MODE_MINIMAL)
/* The log internal thread for log processing can't transfer log well as has too
 * low priority.
 * Dedicated thread for log processing below uses highest application
 * priority. This allows to transmit all logs without adding k_sleep/k_yield
 * anywhere else int the code.
 */

/* most simple log processing theread */
void boot_log_thread_func(void *dummy1, void *dummy2, void *dummy3)
{
    (void)dummy1;
    (void)dummy2;
    (void)dummy3;

     log_init();

     while (1) {
             if (log_process(false) == false) {
                    if (boot_log_stop) {
                        break;
                    }
                    k_sleep(BOOT_LOG_PROCESSING_INTERVAL);
             }
     }

     k_sem_give(&boot_log_sem);
}

void zephyr_boot_log_start(void)
{
        /* start logging thread */
        k_thread_create(&boot_log_thread, boot_log_stack,
                K_THREAD_STACK_SIZEOF(boot_log_stack),
                boot_log_thread_func, NULL, NULL, NULL,
                K_HIGHEST_APPLICATION_THREAD_PRIO, 0,
                BOOT_LOG_PROCESSING_INTERVAL);

        k_thread_name_set(&boot_log_thread, "logging");
}

void zephyr_boot_log_stop(void)
{
    boot_log_stop = true;

    /* wait until log procesing thread expired
     * This can be reworked using a thread_join() API once a such will be
     * available in zephyr.
     * see https://github.com/zephyrproject-rtos/zephyr/issues/21500
     */
    (void)k_sem_take(&boot_log_sem, K_FOREVER);
}
#endif /* defined(CONFIG_LOG) && !defined(ZEPHYR_LOG_MODE_IMMEDIATE) && \
        * !defined(CONFIG_LOG_PROCESS_THREAD) && !defined(ZEPHYR_LOG_MODE_MINIMAL)
        */

#if defined(CONFIG_MCUBOOT_SERIAL) || defined(CONFIG_BOOT_USB_DFU_GPIO)
static bool detect_pin(const char* port, int pin, uint32_t expected, int delay)
{
    int rc;
    int detect_value;
    struct device const *detect_port;

    detect_port = device_get_binding(port);
    __ASSERT(detect_port, "Error: Bad port for boot detection.\n");

    /* The default presence value is 0 which would normally be
     * active-low, but historically the raw value was checked so we'll
     * use the raw interface.
     */
    rc = gpio_pin_configure(detect_port, pin,
                            GPIO_INPUT | GPIO_PULL_UP);
    __ASSERT(rc == 0, "Failed to initialize boot detect pin.\n");

    rc = gpio_pin_get_raw(detect_port, pin);
    detect_value = rc;

    __ASSERT(rc >= 0, "Failed to read boot detect pin.\n");

    if (detect_value == expected) {
        if (delay > 0) {
#ifdef CONFIG_MULTITHREADING
            k_sleep(K_MSEC(50));
#else
            k_busy_wait(50000);
#endif

            /* Get the uptime for debounce purposes. */
            int64_t timestamp = k_uptime_get();

            for(;;) {
                rc = gpio_pin_get_raw(detect_port, pin);
                detect_value = rc;
                __ASSERT(rc >= 0, "Failed to read boot detect pin.\n");

                /* Get delta from when this started */
                uint32_t delta = k_uptime_get() -  timestamp;

                /* If not pressed OR if pressed > debounce period, stop. */
                if (delta >= delay || detect_value != expected) {
                    break;
                }

                /* Delay 1 ms */
#ifdef CONFIG_MULTITHREADING
                k_sleep(K_MSEC(1));
#else
                k_busy_wait(1000);
#endif
            }
        }
    }

    return detect_value == expected;
}
#endif

void main(void)
{
    struct boot_rsp rsp;
    int rc;
    fih_int fih_rc = FIH_FAILURE;

    MCUBOOT_WATCHDOG_FEED();

#if !defined(MCUBOOT_DIRECT_XIP)
    BOOT_LOG_INF("Starting bootloader");
#else
    BOOT_LOG_INF("Starting Direct-XIP bootloader");
#endif

#ifdef CONFIG_MCUBOOT_INDICATION_LED
    /* LED init */
    led_init();
#endif

    os_heap_init();

    ZEPHYR_BOOT_LOG_START();

    (void)rc;

    mcuboot_status_change(MCUBOOT_STATUS_STARTUP);

#if (!defined(CONFIG_XTENSA) && DT_HAS_CHOSEN(zephyr_flash_controller))
    if (!flash_device_get_binding(DT_LABEL(DT_CHOSEN(zephyr_flash_controller)))) {
        BOOT_LOG_ERR("Flash device %s not found",
		     DT_LABEL(DT_CHOSEN(zephyr_flash_controller)));
        while (1)
            ;
    }
#elif (defined(CONFIG_XTENSA) && defined(JEDEC_SPI_NOR_0_LABEL))
    if (!flash_device_get_binding(JEDEC_SPI_NOR_0_LABEL)) {
        BOOT_LOG_ERR("Flash device %s not found", JEDEC_SPI_NOR_0_LABEL);
        while (1)
            ;
    }
#endif

#ifdef CONFIG_MCUBOOT_SERIAL
    if (detect_pin(CONFIG_BOOT_SERIAL_DETECT_PORT,
                   CONFIG_BOOT_SERIAL_DETECT_PIN,
                   CONFIG_BOOT_SERIAL_DETECT_PIN_VAL,
                   CONFIG_BOOT_SERIAL_DETECT_DELAY) &&
            !boot_skip_serial_recovery()) {
#ifdef CONFIG_MCUBOOT_INDICATION_LED
        gpio_pin_set(led, LED0_GPIO_PIN, 1);
#endif

        mcuboot_status_change(MCUBOOT_STATUS_SERIAL_DFU_ENTERED);

        BOOT_LOG_INF("Enter the serial recovery mode");
        rc = boot_console_init();
        __ASSERT(rc == 0, "Error initializing boot console.\n");
        boot_serial_start(&boot_funcs);
        __ASSERT(0, "Bootloader serial process was terminated unexpectedly.\n");
    }
#endif

#if defined(CONFIG_BOOT_USB_DFU_GPIO)
    if (detect_pin(CONFIG_BOOT_USB_DFU_DETECT_PORT,
                   CONFIG_BOOT_USB_DFU_DETECT_PIN,
                   CONFIG_BOOT_USB_DFU_DETECT_PIN_VAL,
                   CONFIG_BOOT_USB_DFU_DETECT_DELAY)) {
#ifdef CONFIG_MCUBOOT_INDICATION_LED
        gpio_pin_set(led, LED0_GPIO_PIN, 1);
#endif

        mcuboot_status_change(MCUBOOT_STATUS_USB_DFU_ENTERED);

        rc = usb_enable(NULL);
        if (rc) {
            BOOT_LOG_ERR("Cannot enable USB");
        } else {
            BOOT_LOG_INF("Waiting for USB DFU");
            wait_for_usb_dfu(K_FOREVER);
            BOOT_LOG_INF("USB DFU wait time elapsed");
        }
    }
#elif defined(CONFIG_BOOT_USB_DFU_WAIT)
    rc = usb_enable(NULL);
    if (rc) {
        BOOT_LOG_ERR("Cannot enable USB");
    } else {
        BOOT_LOG_INF("Waiting for USB DFU");

        mcuboot_status_change(MCUBOOT_STATUS_USB_DFU_WAITING);

        wait_for_usb_dfu(K_MSEC(CONFIG_BOOT_USB_DFU_WAIT_DELAY_MS));
        BOOT_LOG_INF("USB DFU wait time elapsed");

        mcuboot_status_change(MCUBOOT_STATUS_USB_DFU_TIMED_OUT);
    }
#endif

#ifdef CONFIG_BOOT_SERIAL_WAIT_FOR_DFU
    /* Initialize the boot console, so we can already fill up our buffers while
     * waiting for the boot image check to finish. This image check, can take
     * some time, so it's better to reuse thistime to already receive the
     * initial mcumgr command(s) into our buffers
     */
    rc = boot_console_init();
    int timeout_in_ms = CONFIG_BOOT_SERIAL_WAIT_FOR_DFU_TIMEOUT;
    uint32_t start = k_uptime_get_32();
#endif

    FIH_CALL(boot_go, fih_rc, &rsp);

#ifdef CONFIG_BOOT_SERIAL_WAIT_FOR_DFU
    timeout_in_ms -= (k_uptime_get_32() - start);
    if( timeout_in_ms <= 0 ) {
        /* at least one check if time was expired */
        timeout_in_ms = 1;
    }
   boot_serial_check_start(&boot_funcs,timeout_in_ms);
#endif

    if (fih_not_eq(fih_rc, FIH_SUCCESS)) {
        BOOT_LOG_ERR("Unable to find bootable image");

        mcuboot_status_change(MCUBOOT_STATUS_NO_BOOTABLE_IMAGE_FOUND);

        FIH_PANIC;
    }

    BOOT_LOG_INF("Bootloader chainload address offset: 0x%x",
                 rsp.br_image_off);

#if defined(MCUBOOT_DIRECT_XIP)
    BOOT_LOG_INF("Jumping to the image slot");
#else
    BOOT_LOG_INF("Jumping to the first image slot");
#endif

    mcuboot_status_change(MCUBOOT_STATUS_BOOTABLE_IMAGE_FOUND);

    ZEPHYR_BOOT_LOG_STOP();
    do_boot(&rsp);

    mcuboot_status_change(MCUBOOT_STATUS_BOOT_FAILED);

    BOOT_LOG_ERR("Never should get here");
    while (1)
        ;
}
