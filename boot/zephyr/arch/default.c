/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Arm Limited
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 * Copyright (c) 2025 Aerlync Labs Inc.
 * Copyright (c) 2025 Siemens Mobility GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>

#include "bootutil/bootutil_log.h"
#include "bootutil/bootutil.h"
#include "bootutil/fault_injection_hardening.h"
#include "flash_map_backend/flash_map_backend.h"
#include "do_boot.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

/* Default: Assume entry point is at the very beginning of the image. Simply
 * lock interrupts and jump there. This is the right thing to do for X86 and
 * possibly other platforms.
 */
void do_boot(const struct boot_rsp *rsp)
{
	void *start;

#if defined(MCUBOOT_RAM_LOAD)
	start = (void *)(rsp->br_hdr->ih_load_addr + rsp->br_hdr->ih_hdr_size);
#else
	uintptr_t flash_base;
	int rc;

	rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
	if (rc != 0) {
		BOOT_LOG_DBG("Error getting flash device base. rc = %d", rc);
		FIH_PANIC;
	}

	start = (void *)(flash_base + rsp->br_image_off +
			 rsp->br_hdr->ih_hdr_size);
#endif

	/* Lock interrupts and dive into the entry point */
	irq_lock();
	((void (*)(void))start)();
}
