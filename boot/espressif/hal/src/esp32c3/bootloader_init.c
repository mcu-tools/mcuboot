/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_image_format.h"

#include "esp_rom_efuse.h"
#include "esp_rom_gpio.h"
#include "esp_rom_uart.h"
#include "esp_rom_sys.h"

#include "bootloader_init.h"
#include "bootloader_clock.h"
#include "bootloader_flash_config.h"
#include "bootloader_mem.h"
#include "bootloader_flash.h"
#include "bootloader_flash_priv.h"
#include "regi2c_ctrl.h"

#include "soc/extmem_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/efuse_reg.h"
#include "soc/rtc.h"

#include "esp32c3/rom/cache.h"
#include "esp32c3/rom/spi_flash.h"

#include "hal/wdt_hal.h"

extern uint8_t bootloader_common_get_chip_revision(void);

esp_image_header_t WORD_ALIGNED_ATTR bootloader_image_hdr;

void bootloader_clear_bss_section(void)
{
    memset(&_bss_start, 0, (&_bss_end - &_bss_start) * sizeof(_bss_start));
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

static void bootloader_reset_mmu(void)
{
    Cache_Suspend_ICache();
    Cache_Invalidate_ICache_All();
    Cache_MMU_Init();

    REG_CLR_BIT(EXTMEM_ICACHE_CTRL1_REG, EXTMEM_ICACHE_SHUT_IBUS);
    REG_CLR_BIT(EXTMEM_ICACHE_CTRL1_REG, EXTMEM_ICACHE_SHUT_DBUS);
}

esp_err_t bootloader_read_bootloader_header(void)
{
    if (bootloader_flash_read(ESP_BOOTLOADER_OFFSET, &bootloader_image_hdr, sizeof(esp_image_header_t), true) != ESP_OK) {
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
    uint32_t autoload = Cache_Suspend_ICache();
    // Set flash chip size
    esp_rom_spiflash_config_param(rom_spiflash_legacy_data->chip.device_id, size * 0x100000, 0x10000, 0x1000, 0x100, 0xffff);
    Cache_Resume_ICache(autoload);
}

static void IRAM_ATTR bootloader_init_flash_configure(void)
{
    bootloader_flash_dummy_config(&bootloader_image_hdr);
    bootloader_flash_cs_timing_config();
}

static void bootloader_spi_flash_resume(void)
{
    bootloader_execute_flash_command(CMD_RESUME, 0, 0, 0);
    esp_rom_spiflash_wait_idle(&g_rom_flashchip);
}

static esp_err_t bootloader_init_spi_flash(void)
{
    bootloader_init_flash_configure();
    bootloader_spi_flash_resume();
    esp_rom_spiflash_unlock();
    update_flash_config(&bootloader_image_hdr);

    return ESP_OK;
}

static inline void bootloader_hardware_init(void)
{
    // This check is always included in the bootloader so it can
    // print the minimum revision error message later in the boot
    if (bootloader_common_get_chip_revision() < 3) {
        REGI2C_WRITE_MASK(I2C_ULP, I2C_ULP_IR_FORCE_XPD_IPH, 1);
        REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_DREG_1P1_PVT, 12);
    }
}

static inline void bootloader_glitch_reset_disable(void)
{
    /*
      For origin chip & ECO1: only support swt reset;
      For ECO2: fix brownout reset bug, support swt & brownout reset;
      For ECO3: fix clock glitch reset bug, support all reset, include: swt & brownout & clock glitch reset.
    */
    uint8_t chip_version = bootloader_common_get_chip_revision();
    if (chip_version < 2) {
        REG_SET_FIELD(RTC_CNTL_FIB_SEL_REG, RTC_CNTL_FIB_SEL, RTC_CNTL_FIB_SUPER_WDT_RST);
    } else if (chip_version == 2) {
        REG_SET_FIELD(RTC_CNTL_FIB_SEL_REG, RTC_CNTL_FIB_SEL, RTC_CNTL_FIB_SUPER_WDT_RST | RTC_CNTL_FIB_BOR_RST);
    }
}

static void bootloader_super_wdt_auto_feed(void)
{
    REG_WRITE(RTC_CNTL_SWD_WPROTECT_REG, RTC_CNTL_SWD_WKEY_VALUE);
    REG_SET_BIT(RTC_CNTL_SWD_CONF_REG, RTC_CNTL_SWD_AUTO_FEED_EN);
    REG_WRITE(RTC_CNTL_SWD_WPROTECT_REG, 0);
}

void bootloader_config_wdt(void)
{
    wdt_hal_context_t rtc_wdt_ctx = {.inst = WDT_RWDT, .rwdt_dev = &RTCCNTL};
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_set_flashboot_en(&rtc_wdt_ctx, false);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);

#ifdef CONFIG_ESP_MCUBOOT_WDT_ENABLE
    wdt_hal_init(&rtc_wdt_ctx, WDT_RWDT, 0, false);
    uint32_t stage_timeout_ticks = (uint32_t)((uint64_t)CONFIG_BOOTLOADER_WDT_TIME_MS * rtc_clk_slow_freq_get_hz() / 1000);
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_config_stage(&rtc_wdt_ctx, WDT_STAGE0, stage_timeout_ticks, WDT_STAGE_ACTION_RESET_RTC);
    wdt_hal_enable(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
#endif

    wdt_hal_context_t wdt_ctx = {.inst = WDT_MWDT0, .mwdt_dev = &TIMERG0};
    wdt_hal_write_protect_disable(&wdt_ctx);
    wdt_hal_set_flashboot_en(&wdt_ctx, false);
    wdt_hal_write_protect_enable(&wdt_ctx);
}

static void bootloader_init_uart_console(void)
{
    const int uart_num = 0;

    esp_rom_install_uart_printf();
    esp_rom_uart_tx_wait_idle(0);
    uint32_t clock_hz = UART_CLK_FREQ_ROM;
    esp_rom_uart_set_clock_baudrate(uart_num, clock_hz, CONFIG_ESP_CONSOLE_UART_BAUDRATE);
}

esp_err_t bootloader_init(void)
{
    esp_err_t ret = ESP_OK;

    bootloader_hardware_init();
    bootloader_glitch_reset_disable();
    bootloader_super_wdt_auto_feed();
    // protect memory region
    bootloader_init_mem();
    /* check that static RAM is after the stack */
    assert(&_bss_start <= &_bss_end);
    assert(&_data_start <= &_data_end);
    // clear bss section
    bootloader_clear_bss_section();
    // reset MMU
    bootloader_reset_mmu();
    // config clock
    bootloader_clock_configure();
    /* initialize uart console, from now on, we can use ets_printf */
    bootloader_init_uart_console();
    // update flash ID
    bootloader_flash_update_id();
    // read bootloader header
    if ((ret = bootloader_read_bootloader_header()) != ESP_OK) {
        goto err;
    }
    // initialize spi flash
    if ((ret = bootloader_init_spi_flash()) != ESP_OK) {
        goto err;
    }
    // config WDT
    bootloader_config_wdt();
err:
    return ret;
}
