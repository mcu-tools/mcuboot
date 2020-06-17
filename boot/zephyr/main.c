/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
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
#include <drivers/gpio.h>
#include <sys/__assert.h>
#include <drivers/flash.h>
#include <drivers/timer/system_timer.h>
#include <usb/usb_device.h>
#include <soc.h>

#include "target.h"

#include "bootutil/bootutil_log.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "flash_map_backend/flash_map_backend.h"

#ifdef CONFIG_MCUBOOT_SERIAL
#include "boot_serial/boot_serial.h"
#include "serial_adapter/serial_adapter.h"

const struct boot_uart_funcs boot_funcs = {
    .read = console_read,
    .write = console_write
};
#endif

#ifdef CONFIG_BOOT_WAIT_FOR_USB_DFU
#include <usb/class/usb_dfu.h>
#endif

#if CONFIG_MCUBOOT_CLEANUP_ARM_CORE
#include <arm_cleanup.h>
#endif

#if defined(CONFIG_LOG) && !defined(CONFIG_LOG_IMMEDIATE)
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
#endif /* defined(CONFIG_LOG) && !defined(CONFIG_LOG_IMMEDIATE) */

#ifdef CONFIG_SOC_FAMILY_NRF
#include <hal/nrf_power.h>

static inline bool boot_skip_serial_recovery()
{
#if NRF_POWER_HAS_RESETREAS
    uint32_t rr = nrf_power_resetreas_get(NRF_POWER);

    return !(rr == 0 || (rr & NRF_POWER_RESETREAS_RESETPIN_MASK));
#else
    return false;
#endif
}
#else
static inline bool boot_skip_serial_recovery()
{
    return false;
}
#endif

MCUBOOT_LOG_MODULE_REGISTER(mcuboot);

void os_heap_init(void);

#if defined(CONFIG_ARM)

#ifdef CONFIG_BOOT_INTR_VEC_RELOC

#ifdef CONFIG_SW_VECTOR_RELAY
extern void *_vector_table_pointer;
#define VTOR _vector_table_pointer
#elif CONFIG_CPU_CORTEX_M_HAS_VTOR
#define VTOR SCB->VTOR
#endif

#endif /* CONFIG_BOOT_INTR_VEC_RELOC */

struct arm_vector_table {
    uint32_t msp;
    uint32_t reset;
};

extern void sys_clock_disable(void);

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
    irq_lock();
#ifdef CONFIG_SYS_CLOCK_EXISTS
    sys_clock_disable();
#endif
#ifdef CONFIG_USB
    /* Disable the USB to prevent it from firing interrupts */
    usb_disable();
#endif
#if CONFIG_MCUBOOT_CLEANUP_ARM_CORE
    cleanup_arm_nvic(); /* cleanup NVIC registers */
#endif

#ifdef CONFIG_BOOT_INTR_VEC_RELOC
    VTOR = vt;
#endif

    __set_MSP(vt->msp);
#if CONFIG_MCUBOOT_CLEANUP_ARM_CORE
    __set_CONTROL(0x00); /* application will configures core on its own */
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
    uintptr_t flash_base;
    void *start;
    int rc;

    rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
    assert(rc == 0);

    start = (void *)(flash_base + rsp->br_image_off +
                     rsp->br_hdr->ih_hdr_size);

    /* Lock interrupts and dive into the entry point */
    irq_lock();
    ((void (*)(void))start)();
}
#endif

#if defined(CONFIG_LOG) && !defined(CONFIG_LOG_IMMEDIATE) &&\
    !defined(CONFIG_LOG_PROCESS_THREAD)
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
#endif/* defined(CONFIG_LOG) && !defined(CONFIG_LOG_IMMEDIATE) &&\
        !defined(CONFIG_LOG_PROCESS_THREAD) */

void main(void)
{
    struct boot_rsp rsp;
    int rc;

    BOOT_LOG_INF("Starting bootloader");

    os_heap_init();

    ZEPHYR_BOOT_LOG_START();

#if (!defined(CONFIG_XTENSA) && defined(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL))
    if (!flash_device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL)) {
        BOOT_LOG_ERR("Flash device %s not found",
		     DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
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

    struct device *detect_port;
    uint32_t detect_value = !CONFIG_BOOT_SERIAL_DETECT_PIN_VAL;

    detect_port = device_get_binding(CONFIG_BOOT_SERIAL_DETECT_PORT);
    __ASSERT(detect_port, "Error: Bad port for boot serial detection.\n");

    /* The default presence value is 0 which would normally be
     * active-low, but historically the raw value was checked so we'll
     * use the raw interface.
     */
    rc = gpio_pin_configure(detect_port, CONFIG_BOOT_SERIAL_DETECT_PIN,
#ifdef GPIO_INPUT
                            GPIO_INPUT | GPIO_PULL_UP
#else
                            GPIO_DIR_IN | GPIO_PUD_PULL_UP
#endif
           );
    __ASSERT(rc == 0, "Error of boot detect pin initialization.\n");

#ifdef GPIO_INPUT
    rc = gpio_pin_get_raw(detect_port, CONFIG_BOOT_SERIAL_DETECT_PIN);
    detect_value = rc;
#else
    rc = gpio_pin_read(detect_port, CONFIG_BOOT_SERIAL_DETECT_PIN,
                       &detect_value);
#endif
    __ASSERT(rc >= 0, "Error of the reading the detect pin.\n");
    if (detect_value == CONFIG_BOOT_SERIAL_DETECT_PIN_VAL &&
        !boot_skip_serial_recovery()) {
        BOOT_LOG_INF("Enter the serial recovery mode");
        rc = boot_console_init();
        __ASSERT(rc == 0, "Error initializing boot console.\n");
        boot_serial_start(&boot_funcs);
        __ASSERT(0, "Bootloader serial process was terminated unexpectedly.\n");
    }
#endif

#ifdef CONFIG_BOOT_WAIT_FOR_USB_DFU
    rc = usb_enable(NULL);
    if (rc) {
        BOOT_LOG_ERR("Cannot enable USB");
    } else {
        BOOT_LOG_INF("Waiting for USB DFU");
        wait_for_usb_dfu();
        BOOT_LOG_INF("USB DFU wait time elapsed");
    }
#endif

    rc = boot_go(&rsp);
    if (rc != 0) {
        BOOT_LOG_ERR("Unable to find bootable image");
        while (1)
            ;
    }

    BOOT_LOG_INF("Bootloader chainload address offset: 0x%x",
                 rsp.br_image_off);

    BOOT_LOG_INF("Jumping to the first image slot");
    ZEPHYR_BOOT_LOG_STOP();
    do_boot(&rsp);

    BOOT_LOG_ERR("Never should get here");
    while (1)
        ;
}
