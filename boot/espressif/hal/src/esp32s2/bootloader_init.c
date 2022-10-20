/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_image_format.h"

#include "esp_rom_efuse.h"
#include "esp_rom_gpio.h"

#include "bootloader_init.h"
#include "bootloader_common.h"
#include "bootloader_console.h"
#include "bootloader_mem.h"
#include "bootloader_clock.h"
#include "bootloader_flash_config.h"
#include "bootloader_flash.h"
#include "bootloader_flash_priv.h"

#include "soc/dport_reg.h"
#include "soc/efuse_reg.h"
#include "soc/rtc.h"
#include "soc/extmem_reg.h"
#include "soc/io_mux_reg.h"

#include "bootloader_wdt.h"
#include "hal/wdt_hal.h"

#include "esp32s2/rom/cache.h"
#include "esp32s2/rom/ets_sys.h"
#include "esp32s2/rom/spi_flash.h"
#include "esp32s2/rom/uart.h"

extern esp_image_header_t WORD_ALIGNED_ATTR bootloader_image_hdr;

static void bootloader_reset_mmu(void)
{
    Cache_Suspend_ICache();
    Cache_Invalidate_ICache_All();
    Cache_MMU_Init();

    /* normal ROM boot exits with DROM0 cache unmasked,
    but serial bootloader exits with it masked. */
    REG_CLR_BIT(EXTMEM_PRO_ICACHE_CTRL1_REG, EXTMEM_PRO_ICACHE_MASK_DROM0);
}

static void update_flash_config(const esp_image_header_t *bootloader_hdr)
{
    uint32_t size;
    switch (bootloader_hdr->spi_size) {
    case ESP_IMAGE_FLASH_SIZE_1MB:
        size = 1;
        break;
    case ESP_IMAGE_FLASH_SIZE_2MB:
        size = 2;
        break;
    case ESP_IMAGE_FLASH_SIZE_4MB:
        size = 4;
        break;
    case ESP_IMAGE_FLASH_SIZE_8MB:
        size = 8;
        break;
    case ESP_IMAGE_FLASH_SIZE_16MB:
        size = 16;
        break;
    default:
        size = 2;
    }
    uint32_t autoload = Cache_Suspend_ICache();
    // Set flash chip size
    esp_rom_spiflash_config_param(g_rom_flashchip.device_id, size * 0x100000, 0x10000, 0x1000, 0x100, 0xffff);
    Cache_Resume_ICache(autoload);
}

void IRAM_ATTR bootloader_configure_spi_pins(int drv)
{
    const uint32_t spiconfig = esp_rom_efuse_get_flash_gpio_info();
    uint8_t wp_pin = esp_rom_efuse_get_flash_wp_gpio();
    uint8_t clk_gpio_num = SPI_CLK_GPIO_NUM;
    uint8_t q_gpio_num   = SPI_Q_GPIO_NUM;
    uint8_t d_gpio_num   = SPI_D_GPIO_NUM;
    uint8_t cs0_gpio_num = SPI_CS0_GPIO_NUM;
    uint8_t hd_gpio_num  = SPI_HD_GPIO_NUM;
    uint8_t wp_gpio_num  = SPI_WP_GPIO_NUM;
    if (spiconfig != 0) {
        clk_gpio_num = spiconfig         & 0x3f;
        q_gpio_num = (spiconfig >> 6)    & 0x3f;
        d_gpio_num = (spiconfig >> 12)   & 0x3f;
        cs0_gpio_num = (spiconfig >> 18) & 0x3f;
        hd_gpio_num = (spiconfig >> 24)  & 0x3f;
        wp_gpio_num = wp_pin;
    }
    esp_rom_gpio_pad_set_drv(clk_gpio_num, drv);
    esp_rom_gpio_pad_set_drv(q_gpio_num,   drv);
    esp_rom_gpio_pad_set_drv(d_gpio_num,   drv);
    esp_rom_gpio_pad_set_drv(cs0_gpio_num, drv);
    if (hd_gpio_num <= MAX_PAD_GPIO_NUM) {
        esp_rom_gpio_pad_set_drv(hd_gpio_num, drv);
    }
    if (wp_gpio_num <= MAX_PAD_GPIO_NUM) {
        esp_rom_gpio_pad_set_drv(wp_gpio_num, drv);
    }
}

static void IRAM_ATTR bootloader_init_flash_configure(void)
{
    bootloader_flash_dummy_config(&bootloader_image_hdr);
    bootloader_flash_cs_timing_config();
}

static esp_err_t bootloader_init_spi_flash(void)
{
    bootloader_init_flash_configure();
    esp_rom_spiflash_unlock();

    update_flash_config(&bootloader_image_hdr);
    return ESP_OK;
}

static void bootloader_super_wdt_auto_feed(void)
{
    REG_SET_BIT(RTC_CNTL_SWD_CONF_REG, RTC_CNTL_SWD_AUTO_FEED_EN);
}

esp_err_t bootloader_init(void)
{
    esp_err_t ret = ESP_OK;
    bootloader_super_wdt_auto_feed();

    bootloader_init_mem();

    /* check that static RAM is after the stack */
#ifndef NDEBUG
    {
        assert(&_bss_start <= &_bss_end);
        assert(&_data_start <= &_data_end);
    }
#endif
    /* clear bss section */
    bootloader_clear_bss_section();
    /* reset MMU */
    bootloader_reset_mmu();
    /* config clock */
    bootloader_clock_configure();
    /* initialize uart console, from now on, we can use ets_printf */
    bootloader_console_init();
    /* read bootloader header */
    if ((ret = bootloader_read_bootloader_header()) != ESP_OK) {
        goto err;
    }
    // read chip revision and check if it's compatible to bootloader
    if ((ret = bootloader_check_bootloader_validity()) != ESP_OK) {
        goto err;
    }
    /* initialize spi flash */
    if ((ret = bootloader_init_spi_flash()) != ESP_OK) {
        goto err;
    }
    /* config WDT */
    bootloader_config_wdt();
err:
    return ret;
}
