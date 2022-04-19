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

#include "bootloader_init.h"
#include "bootloader_mem.h"
#include "bootloader_clock.h"
#include "bootloader_flash_config.h"
#include "bootloader_flash.h"
#include "bootloader_flash_priv.h"

#include "soc/dport_reg.h"
#include "soc/efuse_reg.h"
#include "soc/rtc.h"

#include "bootloader_wdt.h"
#include "hal/wdt_hal.h"

#include "esp32/rom/cache.h"
#include "esp32/rom/spi_flash.h"
#include "esp32/rom/uart.h"

extern esp_image_header_t WORD_ALIGNED_ATTR bootloader_image_hdr;

static void bootloader_common_vddsdio_configure(void)
{
    rtc_vddsdio_config_t cfg = rtc_vddsdio_get_config();
    if (cfg.enable == 1 && cfg.tieh == RTC_VDDSDIO_TIEH_1_8V) {    /* VDDSDIO regulator is enabled @ 1.8V */
        cfg.drefh = 3;
        cfg.drefm = 3;
        cfg.drefl = 3;
        cfg.force = 1;
        rtc_vddsdio_set_config(cfg);
        ets_delay_us(10); /* wait for regulator to become stable */
    }
}

static void bootloader_reset_mmu(void)
{
    /* completely reset MMU in case serial bootloader was running */
    Cache_Read_Disable(0);
#if !CONFIG_FREERTOS_UNICORE
    Cache_Read_Disable(1);
#endif
    Cache_Flush(0);
#if !CONFIG_FREERTOS_UNICORE
    Cache_Flush(1);
#endif
    mmu_init(0);
#if !CONFIG_FREERTOS_UNICORE
    /* The lines which manipulate DPORT_APP_CACHE_MMU_IA_CLR bit are
        necessary to work around a hardware bug. */
    DPORT_REG_SET_BIT(DPORT_APP_CACHE_CTRL1_REG, DPORT_APP_CACHE_MMU_IA_CLR);
    mmu_init(1);
    DPORT_REG_CLR_BIT(DPORT_APP_CACHE_CTRL1_REG, DPORT_APP_CACHE_MMU_IA_CLR);
#endif

    /* normal ROM boot exits with DROM0 cache unmasked,
        but serial bootloader exits with it masked. */
    DPORT_REG_CLR_BIT(DPORT_PRO_CACHE_CTRL1_REG, DPORT_PRO_CACHE_MASK_DROM0);
#if !CONFIG_FREERTOS_UNICORE
    DPORT_REG_CLR_BIT(DPORT_APP_CACHE_CTRL1_REG, DPORT_APP_CACHE_MASK_DROM0);
#endif
}

static esp_err_t bootloader_check_rated_cpu_clock(void)
{
    int rated_freq = bootloader_clock_get_rated_freq_mhz();
    if (rated_freq < 80) {
        return ESP_FAIL;
    }
    return ESP_OK;
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
    Cache_Read_Disable(0);
    /* Set flash chip size */
    esp_rom_spiflash_config_param(g_rom_flashchip.device_id, size * 0x100000, 0x10000, 0x1000, 0x100, 0xffff);
    /* TODO: set mode */
    /* TODO: set frequency */
    Cache_Flush(0);
    Cache_Read_Enable(0);
}

static void IRAM_ATTR bootloader_init_flash_configure(void)
{
    bootloader_flash_gpio_config(&bootloader_image_hdr);
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

static void bootloader_init_uart_console(void)
{
    const int uart_num = 0;

    uartAttach();
    ets_install_uart_printf();
    uart_tx_wait_idle(0);

    const int uart_baud = CONFIG_ESP_CONSOLE_UART_BAUDRATE;
    uart_div_modify(uart_num, (rtc_clk_apb_freq_get() << 4) / uart_baud);
}

esp_err_t bootloader_init(void)
{
    esp_err_t ret = ESP_OK;

    bootloader_init_mem();

    /* check that static RAM is after the stack */
#ifndef NDEBUG
    {
        assert(&_bss_start <= &_bss_end);
        assert(&_data_start <= &_data_end);
        assert(sp < &_bss_start);
        assert(sp < &_data_start);
    }
#endif
    /* clear bss section */
    bootloader_clear_bss_section();
    /* bootst up vddsdio */
    bootloader_common_vddsdio_configure();
    /* reset MMU */
    bootloader_reset_mmu();
    /* check rated CPU clock */
    if ((ret = bootloader_check_rated_cpu_clock()) != ESP_OK) {
        goto err;
    }
    /* config clock */
    bootloader_clock_configure();
    /* initialize uart console, from now on, we can use ets_printf */
    bootloader_init_uart_console();
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
