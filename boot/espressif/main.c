/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bootutil/bootutil.h>
#include <bootutil/bootutil_log.h>
#include <bootutil/fault_injection_hardening.h>
#include <bootutil/image.h>

#include "bootloader_init.h"

#if defined(CONFIG_EFUSE_VIRTUAL_KEEP_IN_FLASH) || defined(CONFIG_SECURE_BOOT)
#include "esp_efuse.h"
#endif
#ifdef CONFIG_SECURE_BOOT
#include "esp_secure_boot.h"
#endif

#include "esp_loader.h"
#include "os/os_malloc.h"

#ifdef CONFIG_SECURE_BOOT
extern esp_err_t check_and_generate_secure_boot_keys(void);
#endif

void do_boot(struct boot_rsp *rsp)
{
    BOOT_LOG_INF("br_image_off = 0x%x", rsp->br_image_off);
    BOOT_LOG_INF("ih_hdr_size = 0x%x", rsp->br_hdr->ih_hdr_size);
    int slot = (rsp->br_image_off == CONFIG_ESP_APPLICATION_PRIMARY_START_ADDRESS) ? 0 : 1;
    esp_app_image_load(slot, rsp->br_hdr->ih_hdr_size);
}

int main()
{
    bootloader_init();

#ifdef CONFIG_EFUSE_VIRTUAL_KEEP_IN_FLASH
    BOOT_LOG_WRN("eFuse virtual mode is enabled. If Secure boot or Flash encryption is enabled then it does not provide any security. FOR TESTING ONLY!");
    esp_efuse_init_virtual_mode_in_flash(CONFIG_EFUSE_VIRTUAL_OFFSET, CONFIG_EFUSE_VIRTUAL_SIZE);
#endif

#ifdef CONFIG_SECURE_BOOT
    BOOT_LOG_INF("enabling secure boot v2...");

    bool sb_hw_enabled = esp_secure_boot_enabled();

    if (sb_hw_enabled) {
        BOOT_LOG_INF("secure boot v2 is already enabled, continuing..");
    } else {
        esp_efuse_batch_write_begin(); /* Batch all efuse writes at the end of this function */

        esp_err_t err;
        err = check_and_generate_secure_boot_keys();
        if (err != ESP_OK) {
            esp_efuse_batch_write_cancel();
            FIH_PANIC;
        }
    }
#endif

    BOOT_LOG_INF("*** Booting MCUboot build %s ***", MCUBOOT_VER);

    os_heap_init();

    struct boot_rsp rsp;
    fih_int fih_rc = FIH_FAILURE;

    FIH_CALL(boot_go, fih_rc, &rsp);

    if (fih_not_eq(fih_rc, FIH_SUCCESS)) {
        BOOT_LOG_ERR("Unable to find bootable image");
#ifdef CONFIG_SECURE_BOOT
        esp_efuse_batch_write_cancel();
#endif
        FIH_PANIC;
    }

#ifdef CONFIG_SECURE_BOOT
    if (!sb_hw_enabled) {
        BOOT_LOG_INF("blowing secure boot efuse...");
        esp_err_t err;
        err = esp_secure_boot_enable_secure_features();
        if (err != ESP_OK) {
            esp_efuse_batch_write_cancel();
            FIH_PANIC;
        }

        err = esp_efuse_batch_write_commit();
        if (err != ESP_OK) {
            BOOT_LOG_ERR("Error programming security eFuses (err=0x%x).", err);
            FIH_PANIC;
        }

#ifdef CONFIG_SECURE_BOOT_ENABLE_AGGRESSIVE_KEY_REVOKE
        assert(esp_efuse_read_field_bit(ESP_EFUSE_SECURE_BOOT_AGGRESSIVE_REVOKE));
#endif

        assert(esp_secure_boot_enabled());
        BOOT_LOG_INF("Secure boot permanently enabled");
    }
#endif

    do_boot(&rsp);

    while(1);
}
