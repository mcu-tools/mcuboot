/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Arm Limited
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 * Copyright (c) 2025 Aerlync Labs Inc.
 * Copyright (c) 2025 Siemens Mobility GmbH
 * Copyright (c) 2026 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/devicetree/partitions.h>

#include <bootloader_init.h>
#include <esp_image_loader.h>

#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil.h"
#include "do_boot.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#define IMAGE_INDEX_0   0
#define IMAGE_INDEX_1   1

#define PRIMARY_SLOT    0
#define SECONDARY_SLOT  1

#define IMAGE0_PRIMARY_START_ADDRESS \
	DT_PROP_BY_IDX(DT_NODE_BY_PARTITION_LABEL(image_0), reg, 0)
#define IMAGE0_PRIMARY_SIZE \
	DT_PROP_BY_IDX(DT_NODE_BY_PARTITION_LABEL(image_0), reg, 1)

#define IMAGE1_PRIMARY_START_ADDRESS \
	DT_PROP_BY_IDX(DT_NODE_BY_PARTITION_LABEL(image_1), reg, 0)
#define IMAGE1_PRIMARY_SIZE \
	DT_PROP_BY_IDX(DT_NODE_BY_PARTITION_LABEL(image_1), reg, 1)

void do_boot(const struct boot_rsp *rsp)
{
	BOOT_LOG_INF("br_image_off = 0x%x", rsp->br_image_off);
	BOOT_LOG_INF("ih_hdr_size = 0x%x", rsp->br_hdr->ih_hdr_size);

	int slot = (rsp->br_image_off == IMAGE0_PRIMARY_START_ADDRESS) ?
			PRIMARY_SLOT : SECONDARY_SLOT;

	start_cpu0_image(IMAGE_INDEX_0, slot, rsp->br_hdr->ih_hdr_size);
}
