/*
 * Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <flash_map_backend/flash_map_backend.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>

#define SW1_NODE        DT_ALIAS(sw1)
#if DT_NODE_HAS_STATUS(SW1_NODE, okay)
static struct gpio_dt_spec sw1_spec = GPIO_DT_SPEC_GET(SW1_NODE, gpios);
#endif

static int curr_idx = -1;

static uint8_t known_ids[] = {
	FIXED_PARTITION_ID(slot0_partition),
	FIXED_PARTITION_ID(slot1_partition),
};

bool
flash_map_id_get_next(uint8_t *id, bool reset)
{
	if (reset) {
		curr_idx = 0;
#if DT_NODE_HAS_STATUS(SW1_NODE, okay)
		if (gpio_pin_configure_dt(&sw1_spec, GPIO_INPUT) == 0) {
			if (gpio_pin_get_dt(&sw1_spec) == 1) {
				curr_idx = sys_rand8_get() % ARRAY_SIZE(known_ids);
				printk("Booting from curr_idx = %d\n", curr_idx);
			}
		}
#endif
	} else {
		curr_idx++;
	}

	if (curr_idx >= ARRAY_SIZE(known_ids)) {
		return false;
	}

	*id = known_ids[curr_idx];

	return true;
}

bool
flash_map_id_get_current(uint8_t *id)
{
	if (curr_idx == -1 || curr_idx >= ARRAY_SIZE(known_ids)) {
		return false;
	}

	*id = known_ids[curr_idx];

	return true;
}
