/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <bootutil/bootutil_log.h>
#include <bootutil/fault_injection_hardening.h>

#include "bootloader_memory_utils.h"
#include "bootloader_flash_priv.h"
#include "esp_flash_encrypt.h"

#include "rom/uart.h"

#include "esp_mcuboot_image.h"
#include "esp_loader.h"
#include "flash_map_backend/flash_map_backend.h"

#ifdef CONFIG_ESP_MULTI_PROCESSOR_BOOT
#include "app_cpu_start.h"
#endif

#include "esp_rom_sys.h"
#include "esp_cpu.h"

#if CONFIG_IDF_TARGET_ESP32
#define LP_RTC_PREFIX "RTC"
#elif CONFIG_IDF_TARGET_ESP32S2
#define LP_RTC_PREFIX "RTC"
#elif CONFIG_IDF_TARGET_ESP32S3
#define LP_RTC_PREFIX "RTC"
#elif CONFIG_IDF_TARGET_ESP32C2
#elif CONFIG_IDF_TARGET_ESP32C3
#define LP_RTC_PREFIX "RTC"
#elif CONFIG_IDF_TARGET_ESP32C6
#define LP_RTC_PREFIX "LP"
#elif CONFIG_IDF_TARGET_ESP32H2
#define LP_RTC_PREFIX "LP"
#endif

static int load_segment(const struct flash_area *fap, uint32_t data_addr, uint32_t data_len, uint32_t load_addr)
{
    const uint32_t *data = (const uint32_t *)bootloader_mmap((fap->fa_off + data_addr), data_len);
    if (!data) {
        BOOT_LOG_ERR("%s: Bootloader mmap failed", __func__);
        return -1;
    }
    memcpy((void *)load_addr, data, data_len);
    bootloader_munmap(data);
    return 0;
}

void esp_app_image_load(int image_index, int slot, unsigned int hdr_offset, unsigned int *entry_addr)
{
    const struct flash_area *fap;
    int area_id;
    int rc;

    area_id = flash_area_id_from_multi_image_slot(image_index, slot);
    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        BOOT_LOG_ERR("%s: flash_area_open failed with %d", __func__, rc);
    }

    BOOT_LOG_INF("Loading image %d - slot %d from flash, area id: %d", image_index, slot, area_id);

    const uint32_t *data = (const uint32_t *)bootloader_mmap((fap->fa_off + hdr_offset), sizeof(esp_image_load_header_t));
    esp_image_load_header_t load_header = {0};
    memcpy((void *)&load_header, data, sizeof(esp_image_load_header_t));
    bootloader_munmap(data);

    if (load_header.header_magic != ESP_LOAD_HEADER_MAGIC) {
        BOOT_LOG_ERR("Load header magic verification failed. Aborting");
        FIH_PANIC;
    }

    if (!esp_ptr_in_iram((void *)load_header.iram_dest_addr) || !esp_ptr_in_iram((void *)(load_header.iram_dest_addr + load_header.iram_size))) {
        BOOT_LOG_ERR("IRAM region in load header is not valid. Aborting");
        FIH_PANIC;
    }

    if (!esp_ptr_in_dram((void *)load_header.dram_dest_addr) || !esp_ptr_in_dram((void *)(load_header.dram_dest_addr + load_header.dram_size))) {
        BOOT_LOG_ERR("DRAM region in load header is not valid. Aborting");
        FIH_PANIC;
    }

#if SOC_RTC_FAST_MEM_SUPPORTED
    if (load_header.lp_rtc_iram_size > 0) {
        if (!esp_ptr_in_rtc_iram_fast((void *)load_header.lp_rtc_iram_dest_addr) ||
            !esp_ptr_in_rtc_iram_fast((void *)(load_header.lp_rtc_iram_dest_addr + load_header.lp_rtc_iram_size))) {
            BOOT_LOG_ERR("%s_IRAM region in load header is not valid. Aborting", LP_RTC_PREFIX);
            FIH_PANIC;
        }
    }
#endif

#if SOC_RTC_SLOW_MEM_SUPPORTED
    if (load_header.lp_rtc_dram_size > 0) {
        if (!esp_ptr_in_rtc_slow((void *)load_header.lp_rtc_dram_dest_addr) ||
            !esp_ptr_in_rtc_slow((void *)(load_header.lp_rtc_dram_dest_addr + load_header.lp_rtc_dram_size))) {
            BOOT_LOG_ERR("%s_RAM region in load header is not valid. Aborting %p", LP_RTC_PREFIX, load_header.lp_rtc_dram_dest_addr);
            FIH_PANIC;
        }
    }
#endif

    if (!esp_ptr_in_iram((void *)load_header.entry_addr)) {
        BOOT_LOG_ERR("Application entry point (0x%x) is not in IRAM. Aborting", load_header.entry_addr);
        FIH_PANIC;
    }

    BOOT_LOG_INF("DRAM segment: start=0x%x, size=0x%x, vaddr=0x%x", fap->fa_off + load_header.dram_flash_offset, load_header.dram_size, load_header.dram_dest_addr);
    load_segment(fap, load_header.dram_flash_offset, load_header.dram_size, load_header.dram_dest_addr);

    BOOT_LOG_INF("IRAM segment: start=0x%x, size=0x%x, vaddr=0x%x", fap->fa_off + load_header.iram_flash_offset, load_header.iram_size, load_header.iram_dest_addr);
    load_segment(fap, load_header.iram_flash_offset, load_header.iram_size, load_header.iram_dest_addr);

#if SOC_RTC_FAST_MEM_SUPPORTED || SOC_RTC_SLOW_MEM_SUPPORTED
    if (load_header.lp_rtc_dram_size > 0) {
        soc_reset_reason_t reset_reason = esp_rom_get_reset_reason(0);

        /* Unless waking from deep sleep (implying RTC memory is intact), load its segments */
        if (reset_reason != RESET_REASON_CORE_DEEP_SLEEP) {
            BOOT_LOG_INF("%s_RAM segment: paddr=%08xh, vaddr=%08xh, size=%05xh (%6d) load", LP_RTC_PREFIX,
                         (fap->fa_off + load_header.lp_rtc_dram_flash_offset), load_header.lp_rtc_dram_dest_addr,
                         load_header.lp_rtc_dram_size, load_header.lp_rtc_dram_size);
            load_segment(fap, load_header.lp_rtc_dram_flash_offset,
                         load_header.lp_rtc_dram_size, load_header.lp_rtc_dram_dest_addr);
        } else {
            BOOT_LOG_INF("%s_RAM segment: paddr=%08xh, vaddr=%08xh, size=%05xh (%6d) noload", LP_RTC_PREFIX,
                         load_header.lp_rtc_dram_flash_offset, load_header.lp_rtc_dram_dest_addr,
                         load_header.lp_rtc_dram_size, load_header.lp_rtc_dram_size);
        }
    }

    if (load_header.lp_rtc_iram_size > 0) {
        BOOT_LOG_INF("%s_IRAM segment: paddr=%08xh, vaddr=%08xh, size=%05xh (%6d) load", LP_RTC_PREFIX,
                     (fap->fa_off + load_header.lp_rtc_iram_flash_offset), load_header.lp_rtc_iram_dest_addr,
                     load_header.lp_rtc_iram_size, load_header.lp_rtc_iram_size);
        load_segment(fap, load_header.lp_rtc_iram_flash_offset,
                     load_header.lp_rtc_iram_size, load_header.lp_rtc_iram_dest_addr);
    }
#endif

    BOOT_LOG_INF("start=0x%x", load_header.entry_addr);
    uart_tx_wait_idle(0);

    assert(entry_addr != NULL);
    *entry_addr = load_header.entry_addr;
}

void start_cpu0_image(int image_index, int slot, unsigned int hdr_offset)
{
    unsigned int entry_addr;
    esp_app_image_load(image_index, slot, hdr_offset, &entry_addr);
    ((void (*)(void))entry_addr)(); /* Call to application entry address should not return */
    FIH_PANIC; /* It should not get here */
}

#ifdef CONFIG_ESP_MULTI_PROCESSOR_BOOT
void start_cpu1_image(int image_index, int slot, unsigned int hdr_offset)
{
    unsigned int entry_addr;
    esp_app_image_load(image_index, slot, hdr_offset, &entry_addr);
    appcpu_start(entry_addr);
}
#endif
