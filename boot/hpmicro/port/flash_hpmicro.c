/*
 * Copyright (c) 2023 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#define DT_DRV_COMPAT hpmicro_hpm6750_flash_controller
#define SOC_NV_FLASH_NODE DT_INST(0, soc_nv_flash)

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "board.h"
#include "hpm_romapi.h"
#include <board.h>
#include "flash_map.h"
#include "flash.h"
#define LOG_LEVEL CONFIG_FLASH_LOG_LEVEL

#define HPM_STATUS_ZEPHYR_RET(x)    (x)

static xpi_nor_config_t s_xpi_nor_config;

static uint32_t flash_size;
static uint32_t sector_size;
static uint32_t page_size;
static uint32_t block_size;


static const struct flash_parameters flash_hpmicro_parameters = {
    .write_block_size = 4,
    .erase_value = 0xff,
};
static int flash_hpmicro_init(const struct device *dev);
static bool initted = false;
static int flash_hpmicro_read(const struct device *dev, off_t offset,
                void *data,
                size_t size)
{
    hpm_stat_t status = 0;
    if (!initted) {
        initted = true;
        flash_hpmicro_init(dev);
    }
    if (size < 4) {
        uint32_t temp;
        status = rom_xpi_nor_read(BOARD_APP_XPI_NOR_XPI_BASE, xpi_xfer_channel_auto, &s_xpi_nor_config,
                     &temp, offset, 4);
        memcpy(data, &temp, size);
    } else {
        status = rom_xpi_nor_read(BOARD_APP_XPI_NOR_XPI_BASE, xpi_xfer_channel_auto, &s_xpi_nor_config,
                     data, offset, size);
    }

    return HPM_STATUS_ZEPHYR_RET(status);
}

static int flash_hpmicro_write(const struct device *dev, off_t offset,
                 const void *data, size_t size)
{
    hpm_stat_t status = 0;
    if (!initted) {
        initted = true;
        flash_hpmicro_init(dev);
    }
    status = rom_xpi_nor_program(BOARD_APP_XPI_NOR_XPI_BASE, xpi_xfer_channel_auto, &s_xpi_nor_config,
                        data, offset, size);
    return HPM_STATUS_ZEPHYR_RET(status);
}

static int flash_hpmicro_erase(const struct device *dev, off_t offset,
                 size_t size)
{
    hpm_stat_t status = 0;
    if (!initted) {
        initted = true;
        flash_hpmicro_init(dev);
    }
    if (size < 4) {
        while (1) {
        }
    }
    status = rom_xpi_nor_erase(BOARD_APP_XPI_NOR_XPI_BASE, xpi_xfer_channel_auto, &s_xpi_nor_config,
                               offset, size);
    return HPM_STATUS_ZEPHYR_RET(status);
}

static const struct flash_pages_layout flash_hpm_pages_layout[] = {
    {
        .pages_count = HPM_PAGE_LAYOUT_NONE_PAGES_COUNT,
        .pages_size = HPM_PAGE_LAYOUT_NONE_PAGES_SIZE
    },
    {
        .pages_count = HPM_PAGE_LAYOUT_NOR_CFG_PAGES_COUNT,
        .pages_size = HPM_PAGE_LAYOUT_NOR_CFG_PAGES_SIZE
    },
    {
        .pages_count = HPM_PAGE_LAYOUT_BOOT_PARAM_PAGES_COUNT,
        .pages_size = HPM_PAGE_LAYOUT_BOOT_PARAM_PAGES_SIZE
    },
    {
        .pages_count = HPM_PAGE_LAYOUT_MCUBOOT_PAGES_COUNT,
        .pages_size = HPM_PAGE_LAYOUT_MCUBOOT_PAGES_SIZE
    },
    {
        .pages_count = HPM_PAGE_LAYOUT_IMAGE0_SLOT0_PAGES_COUNT,
        .pages_size = HPM_PAGE_LAYOUT_IMAGE0_SLOT0_PAGES_SIZE
    },
    {
        .pages_count = HPM_PAGE_LAYOUT_IMAGE0_SLOT1_PAGES_COUNT,
        .pages_size = HPM_PAGE_LAYOUT_IMAGE0_SLOT1_PAGES_SIZE
    },
    {
        .pages_count = HPM_PAGE_LAYOUT_SCRATCH_PAGES_COUNT,
        .pages_size = HPM_PAGE_LAYOUT_SCRATCH_PAGES_SIZE
    }
};

void flash_hpmicro_page_layout(const struct device *dev,
                 const struct flash_pages_layout **layout,
                 size_t *layout_size)
{
    *layout = flash_hpm_pages_layout;
    *layout_size = ARRAY_SIZE(flash_hpm_pages_layout);
}

static const struct flash_parameters *
flash_hpmicro_get_parameters(const struct device *dev)
{
    return &flash_hpmicro_parameters;
}

static int flash_hpmicro_init(const struct device *dev)
{
    printf("Device %s initialized\n", dev->name);

    xpi_nor_config_option_t option;
    option.header.U = BOARD_APP_XPI_NOR_CFG_OPT_HDR;
    option.option0.U = BOARD_APP_XPI_NOR_CFG_OPT_OPT0;
    option.option1.U = BOARD_APP_XPI_NOR_CFG_OPT_OPT1;

    hpm_stat_t status = rom_xpi_nor_auto_config(BOARD_APP_XPI_NOR_XPI_BASE, &s_xpi_nor_config, &option);
    if (status != status_success) {
        return status;
    }

    rom_xpi_nor_get_property(BOARD_APP_XPI_NOR_XPI_BASE, &s_xpi_nor_config, xpi_nor_property_total_size,
                             &flash_size);
    rom_xpi_nor_get_property(BOARD_APP_XPI_NOR_XPI_BASE, &s_xpi_nor_config, xpi_nor_property_sector_size,
                             &sector_size);
    rom_xpi_nor_get_property(BOARD_APP_XPI_NOR_XPI_BASE, &s_xpi_nor_config, xpi_nor_property_block_size,
                             &block_size);
    rom_xpi_nor_get_property(BOARD_APP_XPI_NOR_XPI_BASE, &s_xpi_nor_config, xpi_nor_property_page_size, &page_size);
    return 0;
}

static const struct flash_driver_api flash_hpmicro_driver_api = {
    .read = flash_hpmicro_read,
    .write = flash_hpmicro_write,
    .erase = flash_hpmicro_erase,
    .get_parameters = flash_hpmicro_get_parameters,
    .page_layout = flash_hpmicro_page_layout,
};

struct device hpm_flash_controller = {
    .name = "hpm flash controller",
    .config = NULL,
    .api = &flash_hpmicro_driver_api,
    .data = NULL,
};