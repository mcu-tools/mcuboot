/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <zephyr.h>
#include <flash.h>
#include <asm_inline.h>

#define BOOT_LOG_LEVEL BOOT_LOG_LEVEL_INFO
#include "bootutil/bootutil_log.h"

#if defined(MCUBOOT_TARGET_CONFIG)
#include MCUBOOT_TARGET_CONFIG
#else
#error "Board is currently not supported by bootloader"
#endif

#include "bootutil/image.h"
#include "bootutil/bootutil.h"

struct device *boot_flash_device;

void os_heap_init(void);

#if defined(CONFIG_ARM)
struct arm_vector_table {
	uint32_t msp;
	uint32_t reset;
};

static void do_boot(struct boot_rsp *rsp)
{
	struct arm_vector_table *vt;

	/* The beginning of the image is the ARM vector table, containing
	 * the initial stack pointer address and the reset vector
	 * consecutively. Manually set the stack pointer and jump into the
	 * reset vector
	 */
	vt = (struct arm_vector_table *)(rsp->br_image_addr +
					 rsp->br_hdr->ih_hdr_size);
	irq_lock();
	_MspSet(vt->msp);
	((void (*)(void))vt->reset)();
}
#else
/* Default: Assume entry point is at the very beginning of the image. Simply
 * lock interrupts and jump there. This is the right thing to do for X86 and
 * possibly other platforms.
 */
static void do_boot(struct boot_rsp *rsp)
{
	void *start;

	start = (void *)(rsp->br_image_addr + rsp->br_hdr->ih_hdr_size);

	/* Lock interrupts and dive into the entry point */
	irq_lock();
	((void (*)(void))start)();
}
#endif

void main(void)
{
	struct boot_rsp rsp;
	int rc;

	BOOT_LOG_INF("Starting bootloader");

	os_heap_init();

	boot_flash_device = device_get_binding(FLASH_DRIVER_NAME);
	if (!boot_flash_device) {
		BOOT_LOG_ERR("Flash device not found");
		while (1)
			;
	}

	rc = boot_go(&rsp);
	if (rc != 0) {
		BOOT_LOG_ERR("Unable to find bootable image");
		while (1)
			;
	}

	BOOT_LOG_INF("Bootloader chainload address: 0x%x", rsp.br_image_addr);
	BOOT_LOG_INF("Jumping to the first image slot");
	do_boot(&rsp);

	BOOT_LOG_ERR("Never should get here");
	while (1)
		;
}
