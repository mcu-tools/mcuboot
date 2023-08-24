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
#include "bootloader_soc.h"

#include "esp_assert.h"

#ifdef CONFIG_MCUBOOT_SERIAL
#include "boot_serial/boot_serial.h"
#include "serial_adapter/serial_adapter.h"

const struct boot_uart_funcs boot_funcs = {
    .read = console_read,
    .write = console_write
};
#endif

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

#define IMAGE_INDEX_0   0
#define IMAGE_INDEX_1   1

#define PRIMARY_SLOT    0
#define SECONDARY_SLOT  1

#ifdef CONFIG_SECURE_BOOT
extern esp_err_t check_and_generate_secure_boot_keys(void);
#endif

void do_boot(struct boot_rsp *rsp)
{
    BOOT_LOG_INF("br_image_off = 0x%x", rsp->br_image_off);
    BOOT_LOG_INF("ih_hdr_size = 0x%x", rsp->br_hdr->ih_hdr_size);
    int slot = (rsp->br_image_off == CONFIG_ESP_IMAGE0_PRIMARY_START_ADDRESS) ? PRIMARY_SLOT : SECONDARY_SLOT;
    start_cpu0_image(IMAGE_INDEX_0, slot, rsp->br_hdr->ih_hdr_size);
}

#ifdef CONFIG_ESP_MULTI_PROCESSOR_BOOT
int read_image_header(uint32_t img_index, uint32_t slot, struct image_header *img_header)
{
    const struct flash_area *fap;
    int area_id;
    int rc = 0;

    area_id = flash_area_id_from_multi_image_slot(img_index, slot);
    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        rc = BOOT_EFLASH;
        goto done;
    }

    if (flash_area_read(fap, 0, img_header, sizeof(struct image_header))) {
        rc = BOOT_EFLASH;
        goto done;
    }

    BOOT_LOG_INF("Image offset = 0x%x", fap->fa_off);
    BOOT_LOG_INF("Image header size = 0x%x", img_header->ih_hdr_size);

done:
    flash_area_close(fap);
    return rc;
}

void do_boot_appcpu(uint32_t img_index, uint32_t slot)
{
    struct image_header img_header;

    if (read_image_header(img_index, slot, &img_header) != 0) {
        FIH_PANIC;
    }

    start_cpu1_image(img_index, slot, img_header.ih_hdr_size);
}
#endif

int main()
{
    if (bootloader_init() != ESP_OK) {
        FIH_PANIC;
    }

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

#if defined(CONFIG_SECURE_BOOT) || defined(CONFIG_SECURE_FLASH_ENC_ENABLED)
    esp_err_t err;
#endif

#ifdef CONFIG_SECURE_BOOT_FLASH_ENC_KEYS_BURN_TOGETHER
    if (esp_secure_boot_enabled() ^ esp_flash_encrypt_initialized_once()) {
        BOOT_LOG_ERR("Secure Boot and Flash Encryption cannot be enabled separately, only together (their keys go into one eFuse key block)");
        FIH_PANIC;
    }

    if (!esp_secure_boot_enabled() || !esp_flash_encryption_enabled()) {
        esp_efuse_batch_write_begin();
    }
#endif // CONFIG_SECURE_BOOT_FLASH_ENC_KEYS_BURN_TOGETHER

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

        err = check_and_generate_secure_boot_keys();
        if (err != ESP_OK) {
            esp_efuse_batch_write_cancel();
            FIH_PANIC;
        }
    }
#endif

    os_heap_init();

    struct boot_rsp rsp;

    FIH_DECLARE(fih_rc, FIH_FAILURE);

#ifdef CONFIG_MCUBOOT_SERIAL
    boot_console_init();
    if (boot_serial_detect_pin()) {
        BOOT_LOG_INF("Enter the serial recovery mode");
        boot_serial_start(&boot_funcs);
    }
#endif

    /* Step 2 (see above for full description):
     *   2) MCUboot validates the application images and prepares the booting process.
     */

    /* MCUboot's boot_go validates and checks all images for update and returns
     * the load information for booting the main image
     */
    FIH_CALL(boot_go, fih_rc, &rsp);
    if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
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

#ifndef CONFIG_SECURE_BOOT_FLASH_ENC_KEYS_BURN_TOGETHER
        assert(esp_secure_boot_enabled());
        BOOT_LOG_INF("Secure boot permanently enabled");
#endif
    }
#endif

#ifdef CONFIG_SECURE_FLASH_ENC_ENABLED
    /* Step 4, 5 & 6 (see above for full description):
     *   4) Generate Flash Encryption key and write to EFUSE.
     *   5) Encrypt flash in-place including bootloader, image primary/secondary slot and scratch.
     *   6) Burn EFUSE to enable flash encryption
     */
    BOOT_LOG_INF("Checking flash encryption...");
    bool flash_encryption_enabled = esp_flash_encrypt_state();
    if (!flash_encryption_enabled) {
#ifdef CONFIG_SECURE_FLASH_REQUIRE_ALREADY_ENABLED
        BOOT_LOG_ERR("flash encryption is not enabled, and SECURE_FLASH_REQUIRE_ALREADY_ENABLED is set, refusing to boot.");
        FIH_PANIC;
#endif // CONFIG_SECURE_FLASH_REQUIRE_ALREADY_ENABLED

        if (esp_flash_encrypt_is_write_protected(true)) {
            FIH_PANIC;
        }

        err = esp_flash_encrypt_init();
        if (err != ESP_OK) {
            BOOT_LOG_ERR("Initialization of Flash Encryption key failed (%d)", err);
            FIH_PANIC;
        }
    }

    if (!flash_encryption_enabled) {
        err = esp_flash_encrypt_contents();
        if (err != ESP_OK) {
            BOOT_LOG_ERR("Encryption flash contents failed (%d)", err);
            FIH_PANIC;
        }

        err = esp_flash_encrypt_enable();
        if (err != ESP_OK) {
            BOOT_LOG_ERR("Enabling of Flash encryption failed (%d)", err);
            FIH_PANIC;
        }
    }

#ifdef CONFIG_SECURE_BOOT_FLASH_ENC_KEYS_BURN_TOGETHER
    if (!esp_secure_boot_enabled() || !flash_encryption_enabled) {
        err = esp_efuse_batch_write_commit();
        if (err != ESP_OK) {
            BOOT_LOG_ERR("Error programming eFuses (err=0x%x).", err);
            FIH_PANIC;
        }
        assert(esp_secure_boot_enabled());
        BOOT_LOG_INF("Secure boot permanently enabled");
    }
#endif // CONFIG_SECURE_BOOT_FLASH_ENC_KEYS_BURN_TOGETHER

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

    /* Disable glitch reset after all the security checks are completed.
     * Glitch detection can be falsely triggered by EMI interference (high RF TX power, etc)
     * and to avoid such false alarms, disable it.
     */
    bootloader_ana_clock_glitch_reset_config(false);

#ifdef CONFIG_ESP_MULTI_PROCESSOR_BOOT
    /* Multi image independent boot
     * Boot on the second processor happens before the image0 boot
     */
    do_boot_appcpu(IMAGE_INDEX_1, PRIMARY_SLOT);
#endif

    do_boot(&rsp);

    while(1);
}
