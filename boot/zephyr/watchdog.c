/*
 * Copyright (c) 2019-2023 MCUboot authors
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 * Copyright (c) 2025 Jamie McCrae
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <watchdog.h>
#include <bootutil/bootutil_log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>

#ifdef CONFIG_BOOT_WATCHDOG_FEED_NRFX_WDT
#include <nrfx_wdt.h>
#endif

BOOT_LOG_MODULE_DECLARE(mcuboot);

__weak void mcuboot_watchdog_setup(void)
{
#if defined(CONFIG_WATCHDOG) && DT_NODE_HAS_STATUS(DT_ALIAS(watchdog0), okay)
    const struct device *watchdog_device = DEVICE_DT_GET(DT_ALIAS(watchdog0));

    if (device_is_ready(watchdog_device)) {
        int rc;

        rc = wdt_setup(watchdog_device, 0);

        if (rc != 0) {
            BOOT_LOG_ERR("Watchdog setup failed: %d", rc);
        }
    }
#endif
}

__weak void mcuboot_watchdog_feed(void)
{
#if CONFIG_BOOT_WATCHDOG_FEED_NRFX_WDT
#define FEED_NRFX_WDT_INST(inst)                             \
    do {                                                     \
        for (uint8_t i = 0; i < NRF_WDT_CHANNEL_NUMBER; i++) \
        {                                                    \
            nrf_wdt_reload_request_set(inst,                 \
                (nrf_wdt_rr_register_t)(NRF_WDT_RR0 + i));   \
        }                                                    \
    } while (0)
#if defined(NRF_WDT0) && defined(NRF_WDT1)
    FEED_NRFX_WDT_INST(NRF_WDT0);
    FEED_NRFX_WDT_INST(NRF_WDT1);
#elif defined(NRF_WDT0)
    FEED_NRFX_WDT_INST(NRF_WDT0);
#elif defined(NRF_WDT30) && defined(NRF_WDT31)
    FEED_NRFX_WDT_INST(NRF_WDT30);
    FEED_NRFX_WDT_INST(NRF_WDT31);
#elif defined(NRF_WDT30)
    FEED_NRFX_WDT_INST(NRF_WDT30);
#elif defined(NRF_WDT31)
    FEED_NRFX_WDT_INST(NRF_WDT31);
#elif defined(NRF_WDT010)
    FEED_NRFX_WDT_INST(NRF_WDT010);
#else
#error "No NRFX WDT instances enabled"
#endif
#elif defined(CONFIG_WATCHDOG) && DT_NODE_HAS_STATUS(DT_ALIAS(watchdog0), okay)
    const struct device *watchdog_device = DEVICE_DT_GET(DT_ALIAS(watchdog0));

    if (device_is_ready(watchdog_device)) {
        wdt_feed(watchdog_device, 0);
    }
#endif
}
