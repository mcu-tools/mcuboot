/*
 * Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "bootutil/bootutil.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/fault_injection_hardening.h"

#define BOOT_TMPBUF_SZ 256

static struct image_header _hdr;
static uint8_t tmpbuf[BOOT_TMPBUF_SZ];

static uint8_t known_ids[] = {
	FIXED_PARTITION_ID(slot0_partition),
	FIXED_PARTITION_ID(slot1_partition),
};

static int current_id;

#define SW1_NODE		DT_ALIAS(sw1)
#if DT_NODE_HAS_STATUS(SW1_NODE, okay)
static struct gpio_dt_spec sw1_spec = GPIO_DT_SPEC_GET(SW1_NODE, gpios);
#endif

fih_ret boot_go_hook(struct boot_rsp *rsp)
{
	int rc;
#ifdef MCUBOOT_RAM_LOAD
	struct boot_loader_state *state;
#endif
	FIH_DECLARE(fih_rc, FIH_FAILURE);
	const struct flash_area *_fa_p;

	current_id = 0;

#if DT_NODE_HAS_STATUS(SW1_NODE, okay)
	if (gpio_pin_configure_dt(&sw1_spec, GPIO_INPUT) == 0) {
		if (gpio_pin_get_dt(&sw1_spec) == 1) {
			current_id = ARRAY_SIZE(known_ids) - 1;
			printk("%s pressed, forcing boot from partition %u\n",
				   sw1_spec.port->name, known_ids[current_id]);
		} else {
			printk("%s not pressed, looping partitions to boot\n",
				   sw1_spec.port->name);
		}
	}
#else
	printk("SW1 not defined, looping partitions to boot\n");
#endif

	for ( ; current_id < ARRAY_SIZE(known_ids); current_id++) {
		printk("Trying to boot from fixed partition %u\n",
			   known_ids[current_id]);

		rc = flash_area_open(known_ids[current_id], &_fa_p);
		if (rc != 0) {
			continue;
		}

		rc = boot_image_load_header(_fa_p, &_hdr);
		if (rc != 0) {
			flash_area_close(_fa_p);
			continue;
		}

#ifdef MCUBOOT_RAM_LOAD
		state = boot_get_loader_state();

		rc = boot_load_image_from_flash_to_sram(state, &_hdr, _fa_p);
		if (rc != 0) {
			flash_area_close(_fa_p);
			continue;
		}
#endif

		FIH_CALL(bootutil_img_validate, fih_rc, NULL, &_hdr, _fa_p, tmpbuf,
				BOOT_TMPBUF_SZ, NULL, 0, NULL);
		if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
			flash_area_close(_fa_p);
#ifdef MCUBOOT_RAM_LOAD
			boot_remove_image_from_sram(state);
#endif
			continue;
		}

		rsp->br_flash_dev_id = flash_area_get_device_id(_fa_p);
		rsp->br_image_off = flash_area_get_off(_fa_p);
		rsp->br_hdr = &_hdr;

		flash_area_close(_fa_p);
		break;
	}

	FIH_RET(fih_rc);
}

int flash_area_id_from_multi_image_slot_hook(int image_index, int slot,
											 int *area_id)
{
	*area_id = known_ids[current_id];

	return 0;
}

int flash_area_get_device_id_hook(const struct flash_area *fa, uint8_t *dev_id)
{
	return BOOT_HOOK_REGULAR;
}
