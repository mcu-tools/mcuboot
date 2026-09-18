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
#include "fault_injection_hardening.h"
#include "flash_map_backend/flash_map_backend.h"
#include "do_boot.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

/*
 * ARC vector table has a pointer to the reset function as the first entry
 * in the vector table. Assume the vector table is at the start of the image,
 * and jump to reset
 */
void do_boot(const struct boot_rsp *rsp)
{
	struct arc_vector_table {
		void (*reset)(void); /* Reset vector */
	} *vt;

#if defined(MCUBOOT_RAM_LOAD)
	vt = (struct arc_vector_table *)(rsp->br_hdr->ih_load_addr + rsp->br_hdr->ih_hdr_size);
#else
	uintptr_t flash_base;
	int rc;

	rc = flash_device_base(rsp->br_flash_dev_id, &flash_base);
	if (rc != 0) {
		BOOT_LOG_DBG("Error getting flash device base. rc = %d", rc);
		FIH_PANIC;
	}

	vt = (struct arc_vector_table *)(flash_base + rsp->br_image_off +
					 rsp->br_hdr->ih_hdr_size);
#endif

	/* Lock interrupts and dive into the entry point */
	irq_lock();
	vt->reset();
}
