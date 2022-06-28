/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "bootloader_init.h"
#include "bootloader_common.h"

#include "bootloader_flash_config.h"
#include "bootloader_flash.h"
#include "bootloader_flash_priv.h"

static const char *TAG = "boot";

esp_image_header_t WORD_ALIGNED_ATTR bootloader_image_hdr;

void bootloader_clear_bss_section(void)
{
    memset(&_bss_start, 0, (&_bss_end - &_bss_start) * sizeof(_bss_start));
}

esp_err_t bootloader_read_bootloader_header(void)
{
    /* load bootloader image header */
    if (bootloader_flash_read(ESP_BOOTLOADER_OFFSET, &bootloader_image_hdr, sizeof(esp_image_header_t), true) != ESP_OK) {
        ESP_LOGE(TAG, "failed to load bootloader image header!");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t bootloader_check_bootloader_validity(void)
{
    /* read chip revision from efuse */
    uint8_t revision = bootloader_common_get_chip_revision();
    ESP_LOGI(TAG, "chip revision: %d", revision);
    /* compare with the one set in bootloader image header */
    if (bootloader_common_check_chip_validity(&bootloader_image_hdr, ESP_IMAGE_BOOTLOADER) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
