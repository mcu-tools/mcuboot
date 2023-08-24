/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <strings.h>
#include "bootloader_flash_priv.h"
#include "bootloader_random.h"
#include "esp_image_format.h"
#include "esp_flash_encrypt.h"
#include "esp_flash_partitions.h"
#include "esp_secure_boot.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_log.h"
#include "hal/wdt_hal.h"
#include "hal/efuse_hal.h"
#include "soc/soc_caps.h"
#ifdef CONFIG_SOC_EFUSE_CONSISTS_OF_ONE_KEY_BLOCK
#include "soc/sensitive_reg.h"
#endif

#include "esp_mcuboot_image.h"

#if CONFIG_IDF_TARGET_ESP32
#define CRYPT_CNT ESP_EFUSE_FLASH_CRYPT_CNT
#define WR_DIS_CRYPT_CNT ESP_EFUSE_WR_DIS_FLASH_CRYPT_CNT
#else
#define CRYPT_CNT ESP_EFUSE_SPI_BOOT_CRYPT_CNT
#define WR_DIS_CRYPT_CNT ESP_EFUSE_WR_DIS_SPI_BOOT_CRYPT_CNT
#endif

#define FLASH_ENC_CNT_MAX (CRYPT_CNT[0]->bit_count)

/* This file implements FLASH ENCRYPTION related APIs to perform
 * various operations such as programming necessary flash encryption
 * eFuses, detect whether flash encryption is enabled (by reading eFuse)
 * and if required encrypt the partitions in flash memory
 */

static const char *TAG = "flash_encrypt";

/* Static functions for stages of flash encryption */
static esp_err_t encrypt_bootloader(void);
static esp_err_t encrypt_primary_slot(void);
static size_t get_flash_encrypt_cnt_value(void);

/**
 * This former inlined function must not be defined in the header file anymore.
 * As it depends on efuse component, any use of it outside of `bootloader_support`,
 * would require the caller component to include `efuse` as part of its `REQUIRES` or
 * `PRIV_REQUIRES` entries.
 * Attribute IRAM_ATTR must be specified for the app build.
 */
bool IRAM_ATTR esp_flash_encryption_enabled(void)
{
#ifndef CONFIG_EFUSE_VIRTUAL_KEEP_IN_FLASH
    return efuse_hal_flash_encryption_enabled();
#else
    uint32_t flash_crypt_cnt = 0;
#if CONFIG_IDF_TARGET_ESP32
    esp_efuse_read_field_blob(ESP_EFUSE_FLASH_CRYPT_CNT, &flash_crypt_cnt, ESP_EFUSE_FLASH_CRYPT_CNT[0]->bit_count);
#else
    esp_efuse_read_field_blob(ESP_EFUSE_SPI_BOOT_CRYPT_CNT, &flash_crypt_cnt, ESP_EFUSE_SPI_BOOT_CRYPT_CNT[0]->bit_count);
#endif
    /* __builtin_parity is in flash, so we calculate parity inline */
    bool enabled = false;
    while (flash_crypt_cnt) {
        if (flash_crypt_cnt & 1) {
            enabled = !enabled;
        }
        flash_crypt_cnt >>= 1;
    }
    return enabled;
#endif // CONFIG_EFUSE_VIRTUAL_KEEP_IN_FLASH
}

static size_t get_flash_encrypt_cnt_value(void)
{
    size_t flash_crypt_cnt = 0;
    esp_efuse_read_field_cnt(CRYPT_CNT, &flash_crypt_cnt);
    return flash_crypt_cnt;
}

bool esp_flash_encrypt_initialized_once(void)
{
    return get_flash_encrypt_cnt_value() != 0;
}

bool esp_flash_encrypt_is_write_protected(bool print_error)
{
    if (esp_efuse_read_field_bit(WR_DIS_CRYPT_CNT)) {
        if (print_error) {
            ESP_LOGE(TAG, "Flash Encryption cannot be enabled (CRYPT_CNT (%d) is write protected)", get_flash_encrypt_cnt_value());
        }
        return true;
    }
    return false;
}

bool esp_flash_encrypt_state(void)
{
    size_t flash_crypt_cnt = get_flash_encrypt_cnt_value();
    bool flash_crypt_wr_dis = esp_flash_encrypt_is_write_protected(false);

    ESP_LOGV(TAG, "CRYPT_CNT %d, write protection %d", flash_crypt_cnt, flash_crypt_wr_dis);

    if (flash_crypt_cnt % 2 == 1) {
        /* Flash is already encrypted */
        int left = (FLASH_ENC_CNT_MAX - flash_crypt_cnt) / 2;
        if (flash_crypt_wr_dis) {
            left = 0; /* can't update FLASH_CRYPT_CNT, no more flashes */
        }
        ESP_LOGI(TAG, "flash encryption is enabled (%d plaintext flashes left)", left);
        return true;
    }
    return false;
}

esp_err_t esp_flash_encrypt_check_and_update(void)
{
    bool flash_encryption_enabled = esp_flash_encrypt_state();
    if (!flash_encryption_enabled) {
#ifndef CONFIG_SECURE_FLASH_REQUIRE_ALREADY_ENABLED
        if (esp_flash_encrypt_is_write_protected(true)) {
            return ESP_FAIL;
        }

        esp_err_t err = esp_flash_encrypt_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Initialization of Flash encryption key failed (%d)", err);
            return err;
        }

        err = esp_flash_encrypt_contents();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Encryption flash contents failed (%d)", err);
            return err;
        }

        err = esp_flash_encrypt_enable();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Enabling of Flash encryption failed (%d)", err);
            return err;
        }
#else
        ESP_LOGE(TAG, "flash encryption is not enabled, and SECURE_FLASH_REQUIRE_ALREADY_ENABLED "
                      "is set, refusing to boot.");
        return ESP_ERR_INVALID_STATE;
#endif // CONFIG_SECURE_FLASH_REQUIRE_ALREADY_ENABLED
    }
    return ESP_OK;
}

static esp_err_t check_and_generate_encryption_keys(void)
{
    size_t key_size = 32;
#ifdef CONFIG_IDF_TARGET_ESP32
    enum { BLOCKS_NEEDED = 1 };
    esp_efuse_purpose_t purposes[BLOCKS_NEEDED] = {
        ESP_EFUSE_KEY_PURPOSE_FLASH_ENCRYPTION,
    };
    esp_efuse_coding_scheme_t coding_scheme = esp_efuse_get_coding_scheme(EFUSE_BLK_ENCRYPT_FLASH);
    if (coding_scheme != EFUSE_CODING_SCHEME_NONE && coding_scheme != EFUSE_CODING_SCHEME_3_4) {
        ESP_LOGE(TAG, "Unknown/unsupported CODING_SCHEME value 0x%x", coding_scheme);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (coding_scheme == EFUSE_CODING_SCHEME_3_4) {
        key_size = 24;
    }
#else
#ifdef CONFIG_SECURE_FLASH_ENCRYPTION_AES256
    enum { BLOCKS_NEEDED = 2 };
    esp_efuse_purpose_t purposes[BLOCKS_NEEDED] = {
        ESP_EFUSE_KEY_PURPOSE_XTS_AES_256_KEY_1,
        ESP_EFUSE_KEY_PURPOSE_XTS_AES_256_KEY_2,
    };
    if (esp_efuse_find_purpose(ESP_EFUSE_KEY_PURPOSE_XTS_AES_128_KEY, NULL)) {
        ESP_LOGE(TAG, "XTS_AES_128_KEY is already in use, XTS_AES_256_KEY_1/2 can not be used");
        return ESP_ERR_INVALID_STATE;
    }
#else
#ifdef CONFIG_SECURE_FLASH_ENCRYPTION_AES128_DERIVED
    enum { BLOCKS_NEEDED = 1 };
    esp_efuse_purpose_t purposes[BLOCKS_NEEDED] = {
        ESP_EFUSE_KEY_PURPOSE_XTS_AES_128_KEY_DERIVED_FROM_128_EFUSE_BITS,
    };
    key_size = 16;
#else
    enum { BLOCKS_NEEDED = 1 };
    esp_efuse_purpose_t purposes[BLOCKS_NEEDED] = {
        ESP_EFUSE_KEY_PURPOSE_XTS_AES_128_KEY,
    };
#endif // CONFIG_SECURE_FLASH_ENCRYPTION_AES128_DERIVED
#endif // CONFIG_SECURE_FLASH_ENCRYPTION_AES256
#endif // CONFIG_IDF_TARGET_ESP32

    /* Initialize all efuse block entries to invalid (max) value */
    esp_efuse_block_t blocks[BLOCKS_NEEDED] = {[0 ... BLOCKS_NEEDED-1] = EFUSE_BLK_KEY_MAX};
    bool has_key = true;
    for (unsigned i = 0; i < BLOCKS_NEEDED; i++) {
        bool tmp_has_key = esp_efuse_find_purpose(purposes[i], &blocks[i]);
        if (tmp_has_key) { // For ESP32: esp_efuse_find_purpose() always returns True, need to check whether the key block is used or not.
            tmp_has_key &= !esp_efuse_key_block_unused(blocks[i]);
        }
        if (i == 1 && tmp_has_key != has_key) {
            ESP_LOGE(TAG, "Invalid efuse key blocks: Both AES-256 key blocks must be set.");
            return ESP_ERR_INVALID_STATE;
        }
        has_key &= tmp_has_key;
    }

    if (!has_key) {
        /* Generate key */
        uint8_t keys[BLOCKS_NEEDED][32] = { 0 };
        ESP_LOGI(TAG, "Generating new flash encryption key...");
        for (unsigned i = 0; i < BLOCKS_NEEDED; ++i) {
            bootloader_fill_random(keys[i], key_size);
        }
        ESP_LOGD(TAG, "Key generation complete");

        esp_err_t err = esp_efuse_write_keys(purposes, keys, BLOCKS_NEEDED);
        if (err != ESP_OK) {
            if (err == ESP_ERR_NOT_ENOUGH_UNUSED_KEY_BLOCKS) {
                ESP_LOGE(TAG, "Not enough free efuse key blocks (need %d) to continue", BLOCKS_NEEDED);
            } else {
                ESP_LOGE(TAG, "Failed to write efuse block with purpose (err=0x%x). Can't continue.", err);
            }
            return err;
        }
    } else {
        for (unsigned i = 0; i < BLOCKS_NEEDED; i++) {
            if (!esp_efuse_get_key_dis_write(blocks[i])
                || !esp_efuse_get_key_dis_read(blocks[i])
                || !esp_efuse_get_keypurpose_dis_write(blocks[i])) { // For ESP32: no keypurpose, it returns always True.
                ESP_LOGE(TAG, "Invalid key state, check read&write protection for key and keypurpose(if exists)");
                return ESP_ERR_INVALID_STATE;
            }
        }
        ESP_LOGI(TAG, "Using pre-loaded flash encryption key in efuse");
    }
    return ESP_OK;
}

esp_err_t esp_flash_encrypt_init(void)
{
    if (esp_flash_encryption_enabled() || esp_flash_encrypt_initialized_once()) {
        return ESP_OK;
    }

    /* Very first flash encryption pass: generate keys, etc. */

    esp_efuse_batch_write_begin(); /* Batch all efuse writes at the end of this function */

    /* Before first flash encryption pass, need to initialise key & crypto config */
    esp_err_t err = check_and_generate_encryption_keys();
    if (err != ESP_OK) {
        esp_efuse_batch_write_cancel();
        return err;
    }

    err = esp_flash_encryption_enable_secure_features();
    if (err != ESP_OK) {
        esp_efuse_batch_write_cancel();
        return err;
    }

    err = esp_efuse_batch_write_commit();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error programming security eFuses (err=0x%x).", err);
        return err;
    }

    return ESP_OK;
}

/* Encrypt all flash data that should be encrypted */
esp_err_t esp_flash_encrypt_contents(void)
{
    esp_err_t err;

#ifdef CONFIG_SOC_EFUSE_CONSISTS_OF_ONE_KEY_BLOCK
    REG_WRITE(SENSITIVE_XTS_AES_KEY_UPDATE_REG, 1);
#endif

    err = encrypt_bootloader();
    if (err != ESP_OK) {
        return err;
    }

    /* If the primary slot executable application is not encrypted,
     * then encrypt it
     */
    err = encrypt_primary_slot();
    if (err != ESP_OK) {
        return err;
    }

    /* Unconditionally encrypts remaining regions
     * This will need changes when implementing multi-slot support
     */
    ESP_LOGI(TAG, "Encrypting remaining flash...");
    uint32_t region_addr = CONFIG_ESP_IMAGE0_SECONDARY_START_ADDRESS;
    size_t region_size = CONFIG_ESP_APPLICATION_SIZE;
    err = esp_flash_encrypt_region(region_addr, region_size);
    if (err != ESP_OK) {
        return err;
    }
    region_addr = CONFIG_ESP_SCRATCH_OFFSET;
    region_size = CONFIG_ESP_SCRATCH_SIZE;
    err = esp_flash_encrypt_region(region_addr, region_size);
    if (err != ESP_OK) {
        return err;
    }

#if defined(CONFIG_ESP_IMAGE_NUMBER) && (CONFIG_ESP_IMAGE_NUMBER == 2)
    region_addr = CONFIG_ESP_IMAGE1_PRIMARY_START_ADDRESS;
    region_size = CONFIG_ESP_APPLICATION_SIZE;
    err = esp_flash_encrypt_region(region_addr, region_size);
    if (err != ESP_OK) {
        return err;
    }
    region_addr = CONFIG_ESP_IMAGE1_SECONDARY_START_ADDRESS;
    region_size = CONFIG_ESP_APPLICATION_SIZE;
    err = esp_flash_encrypt_region(region_addr, region_size);
    if (err != ESP_OK) {
        return err;
    }
#endif

    ESP_LOGI(TAG, "Flash encryption completed");

    return ESP_OK;
}

esp_err_t esp_flash_encrypt_enable(void)
{
    esp_err_t err = ESP_OK;
    if (!esp_flash_encryption_enabled()) {

        if (esp_flash_encrypt_is_write_protected(true)) {
            return ESP_FAIL;
        }

        size_t flash_crypt_cnt = get_flash_encrypt_cnt_value();

#ifdef CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE
        // Go straight to max, permanently enabled
        ESP_LOGI(TAG, "Setting CRYPT_CNT for permanent encryption");
        size_t new_flash_crypt_cnt = FLASH_ENC_CNT_MAX - flash_crypt_cnt;
#else
        /* Set least significant 0-bit in flash_crypt_cnt */
        size_t new_flash_crypt_cnt = 1;
#endif
        ESP_LOGD(TAG, "CRYPT_CNT %d -> %d", flash_crypt_cnt, new_flash_crypt_cnt);
        err = esp_efuse_write_field_cnt(CRYPT_CNT, new_flash_crypt_cnt);

#if defined(CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE) && defined(CONFIG_SOC_FLASH_ENCRYPTION_XTS_AES_128_DERIVED)
        // For AES128_DERIVED, FE key is 16 bytes and XTS_KEY_LENGTH_256 is 0.
        // It is important to protect XTS_KEY_LENGTH_256 from further changing it to 1. Set write protection for this bit.
        // Burning WR_DIS_CRYPT_CNT, blocks further changing of eFuses: DOWNLOAD_DIS_MANUAL_ENCRYPT, SPI_BOOT_CRYPT_CNT, [XTS_KEY_LENGTH_256], SECURE_BOOT_EN.
        esp_efuse_write_field_bit(WR_DIS_CRYPT_CNT);
#endif
    }

    ESP_LOGI(TAG, "Flash encryption completed");

#ifdef CONFIG_EFUSE_VIRTUAL
    ESP_LOGW(TAG, "Flash encryption not really completed. Must disable virtual efuses");
#endif

    return err;
}

static esp_err_t encrypt_bootloader(void)
{
    esp_err_t err;
    uint32_t image_length;
    /* Check for plaintext bootloader (verification will fail if it's already encrypted) */
    if (esp_image_verify_bootloader(&image_length) == ESP_OK) {
        ESP_LOGI(TAG, "Encrypting bootloader...");

        err = esp_flash_encrypt_region(ESP_BOOTLOADER_OFFSET, CONFIG_ESP_BOOTLOADER_SIZE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to encrypt bootloader in place: 0x%x", err);
            return err;
        }
        ESP_LOGI(TAG, "Bootloader encrypted successfully");
    } else {
        ESP_LOGW(TAG, "No valid bootloader was found");
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

static esp_err_t verify_img_header(uint32_t addr, const esp_image_load_header_t *image, bool silent)
{
    esp_err_t err = ESP_OK;

    if (image->header_magic != ESP_LOAD_HEADER_MAGIC) {
        if (!silent) {
            ESP_LOGE(TAG, "image at 0x%x has invalid magic byte",
                     addr);
        }
        err = ESP_ERR_IMAGE_INVALID;
    }

    return err;
}

static esp_err_t encrypt_primary_slot(void)
{
    esp_err_t err;

    esp_image_load_header_t img_header;

    /* Check if the slot is plaintext or encrypted, 0x20 offset is for skipping
     * MCUboot header
     */
    err = bootloader_flash_read(CONFIG_ESP_IMAGE0_PRIMARY_START_ADDRESS + 0x20,
                                &img_header, sizeof(esp_image_load_header_t), true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read slot img header");
        return err;
    } else {
        err = verify_img_header(CONFIG_ESP_IMAGE0_PRIMARY_START_ADDRESS,
                                &img_header, true);
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Encrypting primary slot...");

        err = esp_flash_encrypt_region(CONFIG_ESP_IMAGE0_PRIMARY_START_ADDRESS,
                                       CONFIG_ESP_APPLICATION_SIZE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to encrypt slot in place: 0x%x", err);
            return err;
        }
    } else {
        ESP_LOGW(TAG, "Slot already encrypted or no valid image was found");
    }

    return ESP_OK;
}

esp_err_t esp_flash_encrypt_region(uint32_t src_addr, size_t data_length)
{
    esp_err_t err;
    uint32_t buf[FLASH_SECTOR_SIZE / sizeof(uint32_t)];

    if (src_addr % FLASH_SECTOR_SIZE != 0) {
        ESP_LOGE(TAG, "esp_flash_encrypt_region bad src_addr 0x%x", src_addr);
        return ESP_FAIL;
    }

    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    for (size_t i = 0; i < data_length; i += FLASH_SECTOR_SIZE) {
        wdt_hal_write_protect_disable(&rtc_wdt_ctx);
        wdt_hal_feed(&rtc_wdt_ctx);
        wdt_hal_write_protect_enable(&rtc_wdt_ctx);
        uint32_t sec_start = i + src_addr;
        err = bootloader_flash_read(sec_start, buf, FLASH_SECTOR_SIZE, true);
        if (err != ESP_OK) {
            goto flash_failed;
        }
        err = bootloader_flash_erase_sector(sec_start / FLASH_SECTOR_SIZE);
        if (err != ESP_OK) {
            goto flash_failed;
        }
        err = bootloader_flash_write(sec_start, buf, FLASH_SECTOR_SIZE, true);
        if (err != ESP_OK) {
            goto flash_failed;
        }
    }
    return ESP_OK;

flash_failed:
    ESP_LOGE(TAG, "flash operation failed: 0x%x", err);
    return err;
}
