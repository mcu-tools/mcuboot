/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Arm Limited
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 * Copyright (c) 2025 Aerlync Labs Inc.
 * Copyright (c) 2025 Siemens Mobility GmbH
 * Copyright (c) 2026 Nerijus Bendžiūnas
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

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/usb/usb_device.h>
#include <soc.h>

#include "io/io.h"
#include "target.h"

#include "bootutil/bootutil_log.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/boot_hooks.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/mcuboot_status.h"

#include "do_boot.h"

#if defined(CONFIG_MCUBOOT_UUID_VID) || defined(CONFIG_MCUBOOT_UUID_CID)
#include "bootutil/mcuboot_uuid.h"
#endif /* CONFIG_MCUBOOT_UUID_VID || CONFIG_MCUBOOT_UUID_CID */

#ifdef CONFIG_MCUBOOT_SERIAL
#include "boot_serial/boot_serial.h"
#include "serial_adapter/serial_adapter.h"

const struct boot_uart_funcs boot_funcs = {
    .read = console_read,
    .write = console_write
};
#endif

#if defined(CONFIG_BOOT_USB_DFU_WAIT) || defined(CONFIG_BOOT_USB_DFU_GPIO)
#include <zephyr/usb/class/usb_dfu.h>
#endif

#if defined(CONFIG_LOG)
#include <zephyr/logging/log_ctrl.h>

#if !defined(CONFIG_LOG_MODE_IMMEDIATE) && !defined(CONFIG_LOG_MODE_MINIMAL)
#ifdef CONFIG_LOG_PROCESS_THREAD
#warning "The log internal thread for log processing can't transfer the log"\
         "well for MCUBoot."
#else

#define BOOT_LOG_PROCESSING_INTERVAL K_MSEC(30) /* [ms] */

/* log are processing in custom routine */
K_THREAD_STACK_DEFINE(boot_log_stack, CONFIG_MCUBOOT_LOG_THREAD_STACK_SIZE);
struct k_thread boot_log_thread;
volatile bool boot_log_stop = false;
K_SEM_DEFINE(boot_log_sem, 0, 1);

/* log processing need to be initalized by the application */
#define ZEPHYR_BOOT_LOG_START() zephyr_boot_log_start()
#define ZEPHYR_BOOT_LOG_STOP() zephyr_boot_log_stop()
#endif /* CONFIG_LOG_PROCESS_THREAD */
#endif /* !IMMEDIATE && !MINIMAL */
#endif /* CONFIG_LOG */

#if !defined(ZEPHYR_BOOT_LOG_START)
#define ZEPHYR_BOOT_LOG_START() ((void)0)
#define ZEPHYR_BOOT_LOG_STOP() ((void)0)
#endif

BOOT_LOG_MODULE_REGISTER(mcuboot);

void os_heap_init(void);

#if defined(CONFIG_LOG) && !defined(CONFIG_LOG_MODE_IMMEDIATE) && \
    !defined(CONFIG_LOG_PROCESS_THREAD) && !defined(CONFIG_LOG_MODE_MINIMAL)
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
        if (log_process() == false) {
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
    /* Start logging thread immediately — the polling interval is only
     * used between processing rounds, not as a startup delay.
     */
    k_thread_create(&boot_log_thread, boot_log_stack,
                    K_THREAD_STACK_SIZEOF(boot_log_stack),
                    boot_log_thread_func, NULL, NULL, NULL,
                    K_HIGHEST_APPLICATION_THREAD_PRIO, 0,
                    K_NO_WAIT);

    k_thread_name_set(&boot_log_thread, "logging");
}

void zephyr_boot_log_stop(void)
{
    boot_log_stop = true;

    /* Wake the logging thread immediately if it's in k_sleep().
     * Without this, the thread may sit idle for up to one polling
     * interval before it notices boot_log_stop and drains the
     * final messages.
     */
    k_wakeup(&boot_log_thread);

    /* wait until log procesing thread expired
     * This can be reworked using a thread_join() API once a such will be
     * available in zephyr.
     * see https://github.com/zephyrproject-rtos/zephyr/issues/21500
     */
    (void)k_sem_take(&boot_log_sem, K_FOREVER);

    /* Entice the log backends to flush their contents */
    LOG_PANIC();
}
#endif /* defined(CONFIG_LOG) && !defined(CONFIG_LOG_MODE_IMMEDIATE) && \
        * !defined(CONFIG_LOG_PROCESS_THREAD) && !defined(CONFIG_LOG_MODE_MINIMAL)
        */

#if defined(CONFIG_BOOT_SERIAL_ENTRANCE_GPIO) || defined(CONFIG_BOOT_SERIAL_PIN_RESET) \
    || defined(CONFIG_BOOT_SERIAL_BOOT_MODE) || defined(CONFIG_BOOT_SERIAL_NO_APPLICATION)
static void boot_serial_enter()
{
    int rc;

#ifdef CONFIG_MCUBOOT_INDICATION_LED
    io_led_set(1);
#endif

    mcuboot_status_change(MCUBOOT_STATUS_SERIAL_DFU_ENTERED);

    BOOT_LOG_INF("Enter the serial recovery mode");
    rc = boot_console_init();
    if (rc != 0) {
        BOOT_LOG_DBG("Error initializing boot console. rc = %d", rc);
        FIH_PANIC;
    }

    boot_serial_start(&boot_funcs);
    BOOT_LOG_DBG("Bootloader serial process was terminated unexpectedly");
    FIH_PANIC;
}
#endif

int main(void)
{
    struct boot_rsp rsp;
    int rc;
#if defined(CONFIG_BOOT_USB_DFU_GPIO) || defined(CONFIG_BOOT_USB_DFU_WAIT)
    bool usb_dfu_requested = false;
    bool usb_dfu_forever = false;
#endif
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    MCUBOOT_WATCHDOG_SETUP();
    MCUBOOT_WATCHDOG_FEED();

#if !defined(MCUBOOT_DIRECT_XIP)
    BOOT_LOG_INF("Starting bootloader");
#else
    BOOT_LOG_INF("Starting Direct-XIP bootloader");
#endif

#ifdef CONFIG_MCUBOOT_INDICATION_LED
    /* LED init */
    io_led_init();
#endif

    os_heap_init();

    ZEPHYR_BOOT_LOG_START();

    (void)rc;

    mcuboot_status_change(MCUBOOT_STATUS_STARTUP);

#if defined(CONFIG_MCUBOOT_UUID_VID) || defined(CONFIG_MCUBOOT_UUID_CID)
    FIH_CALL(boot_uuid_init, fih_rc);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        BOOT_LOG_ERR("Unable to initialize UUID module: %d", fih_rc);
        FIH_PANIC;
    }
#endif /* CONFIG_MCUBOOT_UUID_VID || CONFIG_MCUBOOT_UUID_CID */

#ifdef CONFIG_BOOT_SERIAL_ENTRANCE_GPIO
    BOOT_LOG_DBG("Checking GPIO for serial recovery");
    if (io_detect_pin() &&
            !io_boot_skip_serial_recovery()) {
        boot_serial_enter();
    }
#endif

#ifdef CONFIG_BOOT_SERIAL_PIN_RESET
    BOOT_LOG_DBG("Checking RESET pin for serial recovery");
    if (io_detect_pin_reset()) {
        boot_serial_enter();
    }
#endif

#if defined(CONFIG_BOOT_USB_DFU_GPIO)
    BOOT_LOG_DBG("Checking GPIO for USB DFU request");
    if (io_detect_pin()) {
        BOOT_LOG_DBG("Entering USB DFU");

        usb_dfu_requested = true;
        usb_dfu_forever = true;

#ifdef CONFIG_MCUBOOT_INDICATION_LED
        io_led_set(1);
#endif

        mcuboot_status_change(MCUBOOT_STATUS_USB_DFU_ENTERED);
    }
#endif

#if defined(CONFIG_BOOT_USB_DFU_WAIT)
    usb_dfu_requested = true;
#endif

#if defined(CONFIG_BOOT_USB_DFU_GPIO) || defined(CONFIG_BOOT_USB_DFU_WAIT)
    if (usb_dfu_requested) {
        rc = usb_enable(NULL);
        if (rc) {
            BOOT_LOG_ERR("Cannot enable USB: %d", rc);
        } else {
            BOOT_LOG_INF("Waiting for USB DFU");

            if (usb_dfu_forever) {
                wait_for_usb_dfu(K_FOREVER);
                BOOT_LOG_INF("USB DFU wait terminated");
            }
#if defined(CONFIG_BOOT_USB_DFU_WAIT)
            else {
                BOOT_LOG_DBG("Waiting for USB DFU for %dms", CONFIG_BOOT_USB_DFU_WAIT_DELAY_MS);
                mcuboot_status_change(MCUBOOT_STATUS_USB_DFU_WAITING);
                wait_for_usb_dfu(K_MSEC(CONFIG_BOOT_USB_DFU_WAIT_DELAY_MS));
                BOOT_LOG_INF("USB DFU wait time elapsed");
                mcuboot_status_change(MCUBOOT_STATUS_USB_DFU_TIMED_OUT);
            }
#endif
        }
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

#ifdef CONFIG_MCUBOOT_INDICATION_LED
    io_led_set(1);
#endif
#endif

    BOOT_HOOK_GO_CALL_FIH(boot_go_hook, FIH_BOOT_HOOK_REGULAR, fih_rc, &rsp);
    if (FIH_EQ(fih_rc, FIH_BOOT_HOOK_REGULAR)) {
        FIH_CALL(boot_go, fih_rc, &rsp);
    }
    BOOT_LOG_DBG("Left boot_go with success == %d", FIH_EQ(fih_rc, FIH_SUCCESS) ? 1 : 0);

#ifdef CONFIG_BOOT_SERIAL_BOOT_MODE
    if (io_detect_boot_mode()) {
        /* Boot mode to stay in bootloader, clear status and enter serial
         * recovery mode
         */
        BOOT_LOG_DBG("Staying in serial recovery");
        boot_serial_enter();
    }
#endif

#ifdef CONFIG_BOOT_SERIAL_WAIT_FOR_DFU
    timeout_in_ms -= (k_uptime_get_32() - start);
    if( timeout_in_ms <= 0 ) {
        /* at least one check if time was expired */
        timeout_in_ms = 1;
    }
    boot_serial_check_start(&boot_funcs,timeout_in_ms);

#ifdef CONFIG_MCUBOOT_INDICATION_LED
    io_led_set(0);
#endif
#endif

    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
        BOOT_LOG_ERR("Unable to find bootable image");

        mcuboot_status_change(MCUBOOT_STATUS_NO_BOOTABLE_IMAGE_FOUND);

#ifdef CONFIG_BOOT_SERIAL_NO_APPLICATION
        /* No bootable image and configuration set to remain in serial
         * recovery mode
         */
        boot_serial_enter();
#elif defined(CONFIG_BOOT_USB_DFU_NO_APPLICATION)
        rc = usb_enable(NULL);
        if (rc && rc != -EALREADY) {
            BOOT_LOG_ERR("Cannot enable USB");
        } else {
            BOOT_LOG_INF("Waiting for USB DFU");
            wait_for_usb_dfu(K_FOREVER);
        }
#endif

        FIH_PANIC;
    }

#ifdef MCUBOOT_RAM_LOAD
    BOOT_LOG_INF("Bootloader chainload address offset: 0x%x",
                 rsp.br_hdr->ih_load_addr);
#else
    BOOT_LOG_INF("Bootloader chainload address offset: 0x%x",
                 rsp.br_image_off);
#endif

    BOOT_LOG_INF("Image version: v%d.%d.%d", rsp.br_hdr->ih_ver.iv_major,
                                                    rsp.br_hdr->ih_ver.iv_minor,
                                                    rsp.br_hdr->ih_ver.iv_revision);

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
