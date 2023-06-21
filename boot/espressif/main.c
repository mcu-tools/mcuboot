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
#include "bootloader_utility.h"
#include "bootloader_random.h"

#if defined(CONFIG_EFUSE_VIRTUAL_KEEP_IN_FLASH) || defined(CONFIG_SECURE_BOOT)
#include "esp_efuse.h"
#endif
#ifdef CONFIG_SECURE_BOOT
#include "esp_secure_boot.h"
#endif
#ifdef CONFIG_SECURE_FLASH_ENC_ENABLED
#include "esp_flash_encrypt.h"
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

    BOOT_LOG_INF("Enabling RNG early entropy source...");
    bootloader_random_enable();

    /* Rough steps for a first boot when Secure Boot and/or Flash Encryption are still disabled on device:
     * Secure Boot:
     *   1) Calculate the SHA-256 hash digest of the public key and write to EFUSE.
     *   2) Validate the application images and prepare the booting process.
     *   3) Burn EFUSE to enable Secure Boot V2 (ABS_DONE_0).
     * Flash Encryption:
     *   4) Generate Flash Encryption key and write to EFUSE.
     *   5) Encrypt flash in-place including bootloader, image primary/secondary slot and scratch.
     *   6) Burn EFUSE to enable Flash Encryption.
     *   7) Reset system to ensure Flash Encryption cache resets properly.
     */

#ifdef CONFIG_EFUSE_VIRTUAL_KEEP_IN_FLASH
    BOOT_LOG_WRN("eFuse virtual mode is enabled. If Secure boot or Flash encryption is enabled then it does not provide any security. FOR TESTING ONLY!");
    esp_efuse_init_virtual_mode_in_flash(CONFIG_EFUSE_VIRTUAL_OFFSET, CONFIG_EFUSE_VIRTUAL_SIZE);
#endif

#ifdef CONFIG_SECURE_BOOT
    /* Steps 1 (see above for full description):
     *   1) Compute digest of the public key.
     */

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

    /* Step 2 (see above for full description):
     *   2) MCUboot validates the application images and prepares the booting process.
     */

    FIH_CALL(boot_go, fih_rc, &rsp);

    if (fih_not_eq(fih_rc, FIH_SUCCESS)) {
        BOOT_LOG_ERR("Unable to find bootable image");
#ifdef CONFIG_SECURE_BOOT
        esp_efuse_batch_write_cancel();
#endif
        FIH_PANIC;
    }

#ifdef CONFIG_SECURE_BOOT
    /* Step 3 (see above for full description):
     *   3) Burn EFUSE to enable Secure Boot V2.
     */

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

#ifdef CONFIG_SECURE_FLASH_ENC_ENABLED
    /* Step 4, 5 & 6 (see above for full description):
     *   4) Generate Flash Encryption key and write to EFUSE.
     *   5) Encrypt flash in-place including bootloader, image primary/secondary slot and scratch.
     *   6) Burn EFUSE to enable flash encryption
     */

    int rc;

    BOOT_LOG_INF("Checking flash encryption...");
    bool flash_encryption_enabled = esp_flash_encryption_enabled();
    rc = esp_flash_encrypt_check_and_update();
    if (rc != ESP_OK) {
        BOOT_LOG_ERR("Flash encryption check failed (%d).", rc);
        FIH_PANIC;
    }

    /* Step 7 (see above for full description):
     *   7) Reset system to ensure flash encryption cache resets properly.
     */
    if (!flash_encryption_enabled && esp_flash_encryption_enabled()) {
        BOOT_LOG_INF("Resetting with flash encryption enabled...");
        bootloader_reset();
    }
#endif

    BOOT_LOG_INF("Disabling RNG early entropy source...");
    bootloader_random_disable();

    do_boot(&rsp);

    while(1);
}
