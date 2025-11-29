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

/*
 * Channel of watchdog that is setup and fed, if CONFIG_BOOT_WATCHDOG_INSTALL_TIMEOUT_AT_BOOT is
 * not set then this will remain at the default of 0 but the driver will not have been set up
 * which is non-compliant with the Zephyr watchdog driver interface but is left as some drivers
 * may have a hardware (or previously started watchdog) which the driver will automatically read
 * the configuration of upon driver init.
 */
#if defined(CONFIG_BOOT_WATCHDOG_INSTALL_TIMEOUT_AT_BOOT) && DT_NODE_HAS_STATUS(DT_ALIAS(watchdog0), okay)
static int watchdog_channel = 0;
#endif

__weak void mcuboot_watchdog_setup(void)
{
#if defined(CONFIG_BOOT_WATCHDOG_SETUP_AT_BOOT) && DT_NODE_HAS_STATUS(DT_ALIAS(watchdog0), okay)
    const struct device *watchdog_device = DEVICE_DT_GET(DT_ALIAS(watchdog0));

    if (device_is_ready(watchdog_device)) {
        int rc;
#if defined(CONFIG_BOOT_WATCHDOG_INSTALL_TIMEOUT_AT_BOOT)
        struct wdt_timeout_cfg wdt_config = {
                .flags = WDT_FLAG_RESET_SOC,
                .window.min = 0U,
                .window.max = CONFIG_BOOT_WATCHDOG_TIMEOUT_MS,
        };

        rc = wdt_install_timeout(watchdog_device, &wdt_config);

        if (rc >= 0) {
            watchdog_channel = rc;
#endif
            rc = wdt_setup(watchdog_device, 0);

            if (rc != 0) {
                BOOT_LOG_ERR("Watchdog setup failed: %d", rc);
            }
#if defined(CONFIG_BOOT_WATCHDOG_INSTALL_TIMEOUT_AT_BOOT)
        } else {
            BOOT_LOG_ERR("Watchdog install timeout failed: %d", rc);
        }
#endif
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
        wdt_feed(watchdog_device, watchdog_channel);
    }
#endif
}
