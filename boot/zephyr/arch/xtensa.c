/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Arm Limited
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 * Copyright (c) 2025 Aerlync Labs Inc.
 * Copyright (c) 2025 Siemens Mobility GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/flash_map.h>

#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil.h"
#include "do_boot.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

#define SRAM_BASE_ADDRESS	0xBE030000

static void copy_img_to_SRAM(int slot, unsigned int hdr_offset)
{
	const struct flash_area *fap;
	int area_id;
	int rc;
	unsigned char *dst = (unsigned char *)(SRAM_BASE_ADDRESS + hdr_offset);

	BOOT_LOG_INF("Copying image to SRAM");

	area_id = flash_area_id_from_image_slot(slot);
	rc = flash_area_open(area_id, &fap);
	if (rc != 0) {
		BOOT_LOG_ERR("flash_area_open failed with %d", rc);
		goto done;
	}

	rc = flash_area_read(fap, hdr_offset, dst, fap->fa_size - hdr_offset);
	if (rc != 0) {
		BOOT_LOG_ERR("flash_area_read failed with %d", rc);
		goto done;
	}

done:
	flash_area_close(fap);
}

void do_boot(const struct boot_rsp *rsp)
{
	void *start;

	BOOT_LOG_INF("br_image_off = 0x%x", rsp->br_image_off);
	BOOT_LOG_INF("ih_hdr_size = 0x%x", rsp->br_hdr->ih_hdr_size);

	copy_img_to_SRAM(0, rsp->br_hdr->ih_hdr_size);

	start = (void *)(SRAM_BASE_ADDRESS + rsp->br_hdr->ih_hdr_size);
	((void (*)(void))start)();
}
